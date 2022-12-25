//-----------------------------------------------------------------------------
//(c) 2007-2022 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#include <stdio.h>
#include "CDBManager.h"
#include "gzip/gzip.h"
// b2.exe -j8 address-model=32 architecture=x86 link=static threading=multi runtime-link=static --build-type=complete stage --with-iostreams -s ZLIB_SOURCE="F:\vc15-eng\fly-server\trunk\zlib" -s NO_BZIP2=1

#ifdef FLY_SERVER_USE_SQLITE
using sqlite3x::database_error;
using sqlite3x::sqlite3_transaction;
using sqlite3x::sqlite3_reader;
#endif

#ifdef _WIN32
//#define snprintf _snprintf
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket(x) close(x)

#endif
// DECLARE_PERFORMANCE_FILE_STREAM(C:\\!test\\log-sqlite.log, g_flylinkdc_sqlite);

bool g_DisableSQLiteWAL    = false;
//==========================================================================
bool g_setup_log_disable_set       = false;
bool g_setup_log_disable_stat      = false;
bool g_setup_log_disable_download  = false;
bool g_setup_log_disable_download_torrent = false;
bool g_setup_log_disable_antivirus = false;
bool g_setup_log_disable_login     = false;
bool g_setup_log_disable_test_port = false;
bool g_setup_disable_ip_stat       = false;
bool g_setup_log_disable_error_sql = false;
bool g_setup_syslog_disable        = false;
//==========================================================================
CFlyLogThreadInfoArray* CFlyServerContext::g_log_array = NULL;
//========================================================================================================
static std::string g_last_sql;
//========================================================================================================
static void gf_trace_callback(void* p_udp, const char* p_sql)
{
//        char l_time_buf[64];
//        l_time_buf[0] = 0;
//        time_t now;
//        time(&now);
//        strftime(l_time_buf, sizeof(l_time_buf), "%d-%m-%Y %H:%M:%S", gmtime(&now));
//        printf(" [%s] [sqltrace] sql = %s\r\n", l_time_buf, p_sql);
        g_last_sql = p_sql;
}
//==========================================================================
sqlite_int64 get_tick_count()
{
#ifdef _WIN32 // Only in windows
	LARGE_INTEGER l_counter;
	QueryPerformanceCounter(&l_counter);
	return l_counter.QuadPart;
	//return GetTickCount64();
#else // Linux
	struct timeval tim;
	gettimeofday(&tim, NULL);
	unsigned int t = ((tim.tv_sec * 1000) + (tim.tv_usec / 1000)) & 0xffffffff;
	return t;
#endif // _WIN32
}
//==========================================================================
// Пример - 172.23.17.18:30002172.23.17.18: CID = $FLY-TEST-PORT MSL2NL7QB24PKECJEJFPGWY7S3TGTPMXPWDTWPA172.23.17.18:30002
static void set_socket_opt(SOCKET p_sock, int p_option, int p_val)
{
	int len = sizeof(p_val); // x64 - x86 int разный размер
	if(setsockopt(p_sock, SOL_SOCKET, p_option, (char*)&p_val, len) < 0)
  {
      std::cout << "set_socket_opt option = "<< p_option << " val = " << p_val << " failed!\n";
  }
}
//==========================================================================
static void send_udp_tcp_test_port(const std::string& p_PID, const std::string& p_CID, const std::string& p_ip, const string& p_port, bool p_is_tcp)
{
#ifdef _DEBUG
#ifdef _WIN32
	if (p_is_tcp)
	{
		std::cout << "TCP test_port - ip = " << p_ip << ":" << p_port << " CID = " << p_CID << " PID = " << p_PID << std::endl;
	}
	else
	{
		std::cout << "UDP test_port - ip = " << p_ip << ":" << p_port << " CID = " << p_CID << " PID = " << p_PID <<  std::endl;
	}
#endif
#endif
	const unsigned short l_port = atoi(p_port.c_str());
	const string l_header = "$FLY-TEST-PORT " + p_CID + p_ip + ':' + p_port + "|";
	struct sockaddr_in addr = {0};
	int l_result = 0;
	SOCKET sock = socket(AF_INET, p_is_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		std::cout << "send_udp_tcp_test_port - socket error! error code = " << errno << " CID = " << p_CID <<  std::endl;
		return;
	}
  {
    struct timeval timeout;      
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;

    if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        std::cout << "setsockopt SO_RCVTIMEO failed\n";

    if (setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        std::cout << "setsockopt SO_SNDTIMEO failed\n";
  }
	addr.sin_family = AF_INET;
	addr.sin_port = htons(l_port);
	addr.sin_addr.s_addr = inet_addr(p_ip.c_str());
	if (p_is_tcp)
	{
		int sz = sizeof(addr);
		l_result = connect(sock, (struct sockaddr*) &addr, sz);
	}
	if (l_result == SOCKET_ERROR)
	{
		//std::cout << "connect error! error code = " << errno;
		l_result = closesocket(sock);
		if (l_result == SOCKET_ERROR)
		{
			std::cout << "send_udp_tcp_test_port - closesocket error! error code = " << errno << " CID = " << p_CID << std::endl;
		}
		return;
	}
  // TODO - add timeout for TCP
  // http://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections
	l_result = sendto(sock, l_header.c_str(), l_header.size(), 0, (struct sockaddr*)&addr, sizeof(addr));
	if (l_result == SOCKET_ERROR)
	{
		std::cout << "send_udp_tcp_test_port - sendto error! error code = " << errno << " CID = " << p_CID << std::endl;
	}
	l_result = closesocket(sock);
	if (l_result == SOCKET_ERROR)
	{
		std::cout << "send_udp_tcp_test_port - closesocket error! error code = " << errno << " CID = " << p_CID << std::endl;
	}
}
//==========================================================================
static void* thread_proc_tcp_test_port(void* p_param)
{
	static volatile LONG g_count_thread = 0;
	CFlySafeGuard l_call_deep(g_count_thread); // TODO - может считать число потоков через ОС
	string l_str_deep_call(LONG(l_call_deep), '*');
	std::unique_ptr<CFlyPortTestThreadInfo> l_info(reinterpret_cast<CFlyPortTestThreadInfo*>(p_param));
	for (int i = 0; i < l_info->m_ports.size(); ++i)
	{
		send_udp_tcp_test_port(l_info->m_PID, l_info->m_CID, l_info->m_ip, l_info->m_ports[i].first, l_info->m_ports[i].second);
#ifdef _WIN32
		std::cout <<  "[" << l_str_deep_call << "] " <<
		          l_info->get_type_port(i) << " port-thread-test ip = " << l_info->m_ip << ":" << l_info->m_ports[i].first <<
		          " CID = " << l_info->m_CID << " PID = " << l_info->m_PID << std::endl;
#else
		if (g_setup_syslog_disable == false)
		{
			syslog(LOG_NOTICE, "[%s] %s-port-thread-test %s:%s CID = %s PID = %s",
			       l_str_deep_call.c_str(),
			       l_info->get_type_port(i),
			       l_info->m_ip.c_str(),
			       l_info->m_ports[i].first.c_str(),
			       l_info->m_CID.c_str(),
			       l_info->m_PID.c_str());
		}
#endif
	}
	return NULL;
}
//========================================================================================================
static string process_test_port(const CFlyServerContext& p_flyserver_cntx)
{
	string l_result;
	Json::Value l_root;
	Json::Reader reader(Json::Features::strictMode());
	const bool parsingSuccessful = reader.parse(p_flyserver_cntx.m_in_query, l_root);
	if (!parsingSuccessful)
	{
		const char* l_error = "[FLY_POST_QUERY_TEST_PORT] Failed to parse json configuration";
		std::cout  << l_error << std::endl;
#ifndef _WIN32
		syslog(LOG_NOTICE, "[FLY_POST_QUERY_TEST_PORT] Failed to parse json configuration - %s", l_error);
#endif
	}
	else
	{
		const Json::Value& l_udp = l_root["udp"];
		const Json::Value& l_tcp = l_root["tcp"];
		CFlyPortTestThreadInfo* l_info = NULL;
		if (l_tcp.size() || l_udp.size())
		{
			const string l_CID = l_root["CID"].asString();
			const string l_PID = l_root["PID"].asString();
			l_info = new CFlyPortTestThreadInfo;
			l_info->m_ip = p_flyserver_cntx.m_remote_ip;
			l_info->m_CID = l_CID;
			l_info->m_PID = l_PID;
			l_info->m_ports.reserve(l_tcp.size() + l_udp.size());
		}
		if (l_info)
		{
			for (int j = 0; j < l_udp.size(); ++j)
			{
				const std::string l_port = l_udp[j]["port"].asString();
				l_info->m_ports.push_back(std::make_pair(l_port, false));
			}
			for (int k = 0; k < l_tcp.size(); ++k)
			{
				const std::string l_port = l_tcp[k]["port"].asString();
				l_info->m_ports.push_back(std::make_pair(l_port, true));
			}
			// Создадим поток для теста TCP
			if (mg_start_thread(thread_proc_tcp_test_port, l_info))
			{
				delete l_info;
			}
		}
		Json::Value l_test_port_result;
		l_test_port_result["ip"] = p_flyserver_cntx.m_remote_ip;
		l_result = l_test_port_result.toStyledString();
	}
	return l_result;
}
//==========================================================================
static void* thread_proc_store_log(void* p_param)
{
	CFlyLogThreadInfoArray* l_p_array = (CFlyLogThreadInfoArray*)p_param;

	for (CFlyLogThreadInfoArray::iterator i = l_p_array->begin(); i != l_p_array->end(); ++i)
	{
		const char* l_log_dir_name =  NULL;
		switch (i->m_query_type)
		{
#ifdef FLY_SERVER_USE_FULL_LOCAL_LOG
			case FLY_POST_QUERY_LOGIN:
				if (!g_setup_log_disable_login)
					l_log_dir_name = "log-login";
				break;
			case FLY_POST_QUERY_GET:
				l_log_dir_name = "log-get";
				break;
#endif
			case FLY_POST_QUERY_TEST_PORT:
				if (!g_setup_log_disable_test_port)
					l_log_dir_name = "log-test-port";
				break;
			case FLY_POST_QUERY_STATISTIC:
				if (!g_setup_log_disable_stat)
					l_log_dir_name = "log-stat";
				break;
			case FLY_POST_QUERY_ERROR_SQL:
				if (!g_setup_log_disable_error_sql)
					l_log_dir_name = "log-error-sql";
				break;
#ifdef FLY_SERVER_USE_SQLITE
			case FLY_POST_QUERY_SET:
				if (!g_setup_log_disable_set)
					l_log_dir_name = "log-set";
				break;
			case FLY_POST_QUERY_ANTIVIRUS:
				if (!g_setup_log_disable_antivirus)
					l_log_dir_name = "log-antivirus";
				break;
			case FLY_POST_QUERY_DOWNLOAD:
				if (!g_setup_log_disable_download)
					l_log_dir_name = "log-download";
				break;
			case FLY_POST_QUERY_DOWNLOAD_TORRENT:
				if (!g_setup_log_disable_download)
					l_log_dir_name = "log-download-torrent";
				break;
#endif
		}
		if (l_log_dir_name)
		{
			const string l_file_name = CFlyServerContext::get_json_file_name(l_log_dir_name, i->m_remote_ip.c_str(), i->m_now);
			std::fstream l_log_json(l_file_name.c_str(), std::ios_base::out | std::ios_base::trunc);
			if (!l_log_json.is_open())
			{
				std::cout << "Error open file: " << l_file_name << " errno = " << errno << "\r\n";
#ifndef _WIN32
				syslog(LOG_NOTICE, "Error open file: = %s errno = %d", l_file_name.c_str(), errno);
#endif
			}
			else
			{
				if (i->m_in_query.length())
				{
						l_log_json.write(i->m_in_query.c_str(), i->m_in_query.length());
						if (l_log_json.fail() || !l_log_json.good()) 
							{
								std::cout << "Error: failed to write to l_log_json!" << l_file_name << " errno = " << errno <<"\r\n";
#ifndef _WIN32
								syslog(LOG_NOTICE, "Error: failed to write to l_log_json! file = %s errno = %d", l_file_name.c_str(), errno);
#endif
							}
				}
				else
				{
					std::cout << "Error: len(=0) for log file: " << l_file_name << " errno = " << errno << "\r\n";
#ifndef _WIN32
					syslog(LOG_NOTICE, "Error: len(=0) for log file = %s errno = %d", l_file_name.c_str(), errno);
#endif
				}
#ifdef _DEBUG
				if (i->m_query_type != FLY_POST_QUERY_TEST_PORT)
				{
					// TODO - проверить как записывается
					//if (!l_p->l_flyserver_cntx.m_res_stat.empty())
					//{
					//  l_log_json << std::endl << "OUT:" << std::endl << l_flyserver_cntx.m_res_stat;
					//}
				}
#endif
			}
		}
	}
	std::cout << std::endl << "Flush log files count: " << l_p_array->size() << "\r\n";
#ifndef _WIN32
	syslog(LOG_NOTICE, "Flush log files count: = %d", int(l_p_array->size()));
#endif
	delete l_p_array;
	return NULL;
}
//==========================================================================
void CFlyServerContext::run_db_query(const char* p_content, size_t p_len, CDBManager& p_DB)
{
	zlib_uncompress((uint8_t*)p_content, p_len, m_decompress);
	m_tick_count_start_db = get_tick_count();
	init_in_query(p_content, p_len);

#ifdef MT_DEBUG
	if (m_query_type == FLY_POST_QUERY_GET && m_decompress.size() != p_len)
	{
		std::ofstream l_fs;
		static int g_id_file;
		l_fs.open(std::string("flylinkdc-extjson-zlib-file-" + toString(++g_id_file) + ".json.zlib").c_str(), std::ifstream::out);
#ifdef __linux__
//		l_fs.open("flylinkdc-extjson-zlib-file-" + toString(++g_id_file) + ".json.zlib", std::ifstream::out);
#else
//      l_fs.open("flylinkdc-extjson-zlib-file-" + toString(++g_id_file) + ".json.zlib", std::ifstream::out | std::ifstream::binary);
#endif
		l_fs.write(p_content, p_len);
	}
#endif // MT_DEBUG

#ifdef FLY_SERVER_USE_SQLITE
	if (m_query_type == FLY_POST_QUERY_LOGIN)
	{
		m_res_stat = p_DB.login(m_in_query, m_remote_ip);
	}
	else if (is_get_or_set_query())
	{
		m_res_stat = p_DB.store_media_info(*this);
		log_error();
	}
	else if (m_query_type == FLY_POST_QUERY_DOWNLOAD)
	{
		p_DB.add_download_tth(m_in_query);
		extern unsigned long long g_count_download;
		++g_count_download;
	}
	else if (m_query_type == FLY_POST_QUERY_DOWNLOAD_TORRENT)
	{
		p_DB.add_download_torrent(m_in_query);
		extern unsigned long long g_count_download_torrent;
		++g_count_download_torrent;
	}
	else if (m_query_type == FLY_POST_QUERY_ANTIVIRUS)
	{
		p_DB.antivirus_inc(m_in_query);
		extern unsigned long long g_count_antivirus;
		++g_count_antivirus;
	}
	else 
#endif // FLY_SERVER_USE_SQLITE
	if (m_query_type == FLY_POST_QUERY_TEST_PORT)
	{
		m_res_stat = process_test_port(*this);
	}
	m_tick_count_stop_db = get_tick_count();
	run_thread_log();
	comress_result();
}
//==========================================================================
void CFlyServerContext::send_syslog() const
{
	extern unsigned long long g_sum_out_size;
	extern unsigned long long g_sum_in_size;
	extern unsigned long long g_z_sum_out_size;
	extern unsigned long long g_z_sum_in_size;
	extern unsigned long long g_count_query;
	if (g_setup_syslog_disable == false)
	{
		char l_log_buf[512];
		l_log_buf[0]   = 0;// Пока заполняется 152-160
		char l_buf_cache[32];
		l_buf_cache[0] = 0;
		char l_buf_counter[64];
		l_buf_counter[0] = 0;
		if (m_count_cache)
		{
			snprintf(l_buf_cache, sizeof(l_buf_cache), "[cache=%u]", (unsigned) m_count_cache);
		}
		if (m_count_get_only_counter == 0 && m_count_get_base_media_counter == 1 && m_count_get_ext_media_counter == 1)
		{
			snprintf(l_buf_counter, sizeof(l_buf_counter), "%s","[get full Inform!]");
		}
		else if (m_count_get_base_media_counter != 0 || m_count_get_ext_media_counter != 0 || m_count_insert != 0)
		{
			snprintf(l_buf_counter, sizeof(l_buf_counter), "[cnt=%u,base=%u,ext=%u,new=%u]",
			         (unsigned)m_count_get_only_counter,
			         (unsigned)m_count_get_base_media_counter,
			         (unsigned)m_count_get_ext_media_counter,
			         (unsigned)m_count_insert);
		}
		if (m_query_type == FLY_POST_QUERY_TEST_PORT || m_query_type == FLY_POST_QUERY_ERROR_SQL)
		{
			snprintf(l_log_buf, sizeof(l_log_buf), "[%s][%c][%u][%s][%s][in:%u/%u][c:%u][%s]",
			         m_fly_response.c_str(),
			         get_compress_flag(),
			         (unsigned)m_count_file_in_json,
			         m_uri.c_str(),
			         m_remote_ip.c_str(),
			         get_real_query_size(),
			         m_content_len,
					(unsigned)g_count_query,
			         m_user_agent.c_str()
			        );
		}
		else if (m_query_type == FLY_POST_QUERY_LOGIN || m_query_type == FLY_POST_QUERY_STATISTIC)
		{
			snprintf(l_log_buf, sizeof(l_log_buf), "[%s][%c][%u][%s][%s][s_in:%uG/%uG][s_out:%uG/%uG][in:%u/%u][out:%u/%u][c:%u][time db=%u][%s]",
			         m_fly_response.c_str(),
			         get_compress_flag(),
			         m_count_file_in_json,
			         m_uri.c_str(),
			         m_remote_ip.c_str(),
					(unsigned)(g_sum_in_size / 1024 / 1024 / 1024),
					(unsigned)(g_z_sum_in_size / 1024 / 1024 / 1024),
					(unsigned)(g_sum_out_size / 1024 / 1024 / 1024),
					(unsigned)(g_z_sum_out_size / 1024 / 1024 / 1024),
			         get_real_query_size(),
			         m_content_len,
			         (unsigned)m_res_stat.size(),
			         get_http_len(),
					(unsigned)g_count_query,
					(unsigned)get_delta_db(),
			         m_user_agent.c_str()
			        );
		}
		else
		{
			snprintf(l_log_buf, sizeof(l_log_buf), "[%s][%c][%u]%s[%s][%s][%u/%u->%u/%u][time db=%u][%s]%s%s",
			         m_fly_response.c_str(),
			         get_compress_flag(),
			         m_count_file_in_json,
			         m_query_type == FLY_POST_QUERY_SET ? " -> " : "",
			         m_uri.c_str(),
			         m_remote_ip.c_str(),
			         get_real_query_size(),
			         m_content_len,
			         (unsigned)m_res_stat.size(),
			         get_http_len(),
					 (unsigned)get_delta_db(),
			         m_user_agent.c_str(),
			         l_buf_cache,
			         l_buf_counter
			        );
		}
		std::cout << ".";
		static int g_cnt = 0;
		if ((++g_cnt % 30) == 0)
			std::cout << std::endl;
#ifndef _WIN32 // Only in linux
		syslog(LOG_NOTICE, "%s", l_log_buf);
#else
		std::cout << l_log_buf << std::endl;
#endif
	}
}

//==========================================================================
void CFlyServerContext::flush_log_array(bool p_is_force)
{
	if (g_log_array)
	{
#ifndef _DEBUG
		if (g_log_array->size() > 50 || p_is_force)
#else
		if (g_log_array->size() > 0 || p_is_force)
#endif
		{
			CFlyLogThreadInfoArray* l_log_array = g_log_array;
			g_log_array = NULL;
			if (mg_start_thread(thread_proc_store_log, l_log_array))
			{
				thread_proc_store_log(l_log_array); // Выполняем без потока и зачищаем l_thread_param
			}
		}
	}
}
//==========================================================================
void CFlyServerContext::run_thread_log()
{
	if (is_valid_query())
	{
		if (!m_in_query.empty() &&
		        (
		            m_query_type == FLY_POST_QUERY_SET       && g_setup_log_disable_set == false ||
		            m_query_type == FLY_POST_QUERY_LOGIN     && g_setup_log_disable_login == false ||
		            m_query_type == FLY_POST_QUERY_STATISTIC && g_setup_log_disable_stat == false ||
		            m_query_type == FLY_POST_QUERY_ERROR_SQL && g_setup_log_disable_error_sql == false ||
		            m_query_type == FLY_POST_QUERY_TEST_PORT && g_setup_log_disable_test_port == false ||
		            m_query_type == FLY_POST_QUERY_DOWNLOAD  && g_setup_log_disable_download == false ||
					m_query_type == FLY_POST_QUERY_DOWNLOAD_TORRENT  && g_setup_log_disable_download_torrent == false ||
					m_query_type == FLY_POST_QUERY_ANTIVIRUS && g_setup_log_disable_antivirus == false)
		   ) // Есть входной запрос и тип который нужно сохранить на диск
		{
			if (
			    m_query_type != FLY_POST_QUERY_GET &&
			    m_query_type != FLY_POST_QUERY_LOGIN
			)  // В релизе не пишем файл типа get,login и тест портов - экономим ресурсы
			{
				if (!g_log_array)
				{
					g_log_array = new CFlyLogThreadInfoArray;
				}
				g_log_array->push_back(CFlyLogThreadInfo(m_query_type, m_remote_ip, m_in_query));
				flush_log_array(false);
			}
		}
	}
	else
	{
		const char* l_log_text = "l_query_type == FLY_POST_QUERY_GET || l_query_type == FLY_POST_QUERY_SET || "
		                         "l_query_type == FLY_POST_QUERY_LOGIN || l_query_type == FLY_POST_QUERY_STATISTIC || l_query_type == FLY_POST_QUERY_ERROR_SQL || l_query_type == FLY_POST_QUERY_TEST_PORT";
#ifdef _DEBUG
		                         
		std::cout << l_log_text << std::endl;
#ifndef _WIN32
		syslog(LOG_NOTICE, "%s", l_log_text);
#endif
#endif // _DEBUG
	}
}
//========================================================================================================
bool zlib_compress(const char* p_source, size_t p_len, std::vector<unsigned char>& p_compress, int& p_zlib_result, int p_level /*= 9*/)
{
	auto l_dest_length = compressBound(p_len) + 2;
	p_compress.resize(l_dest_length);
	p_zlib_result = compress2(p_compress.data(), &l_dest_length, (uint8_t*)p_source, p_len, p_level);
	if (p_zlib_result == Z_OK) // TODO - Check memory
	{
#ifdef _DEBUG
		if (l_dest_length)
		{
			std::cout << std::endl << "Compress zlib size " << p_len << "/" << l_dest_length << std::endl;
		}
#endif
		p_compress.resize(l_dest_length);
	}
	else
	{
		p_compress.clear();
	}
	return !p_compress.empty();
}
//========================================================================================================
bool zlib_uncompress(const uint8_t* p_zlib_source, size_t p_zlib_len, std::vector<unsigned char>& p_decompress)
{
	uLongf l_decompress_size = p_zlib_len * 3;
	if (p_zlib_len >= 2 && p_zlib_source[0] == 0x78) // zlib контент?
													 // && (unsigned char)p_zlib_source[1] == 0x9C  этот код может быть другим
	{
		// Уровень компрессии - код второго байта (методом подбора)
		// 1     - 0x01
		// [2-5] - 0x5e
		// 6 - 0x9c
		// [7-9] - 0xda
		//			#ifdef _DEBUG
		//					char l_dump_zlib_debug[10] = {0};
		//					sprintf(l_dump_zlib_debug, "%#x", (unsigned char)l_post_data[1] & 0xFF);
		//				    std::cout << "DEBUD zlib decompress header l_post_data[1] = " << l_dump_zlib_debug << std::endl;
		//			#endif

		p_decompress.resize(l_decompress_size);
		while (1)
		{
			const int l_un_compress_result = uncompress(p_decompress.data(), &l_decompress_size, p_zlib_source, p_zlib_len);
			if (l_un_compress_result == Z_BUF_ERROR)
			{
				l_decompress_size *= 2;
				p_decompress.resize(l_decompress_size);
				continue;
			}
			if (l_un_compress_result == Z_OK)
			{
				p_decompress.resize(l_decompress_size);
			}
			else
			{
				p_decompress.clear(); // Если ошибка - зачистим данные. размер мссива является флажком.
									  // TODO оптимизнуть и подменить входной вектор.

				std::cout << "Error zlib_uncompress: code = " << l_un_compress_result << std::endl;
#ifndef _WIN32
				syslog(LOG_NOTICE, "Error zlib_uncompress: code = %d", l_un_compress_result);
#endif
			}
			break;
		};
	}
	return !p_decompress.empty();
}

#ifdef FLY_SERVER_USE_SQLITE
//========================================================================================================
static void* thread_proc_sql_inc_counter_file(void* p_param)
{
	CDBManager* l_db = (CDBManager*)p_param;
	Lock l(l_db->m_cs_sqlite);
	l_db->flush_inc_counter_fly_file();
	return NULL;
}
//========================================================================================================
static void* thread_proc_sql_add_new_file(void* p_param)
{
	CFlyThreadUpdaterInfo* l_p = (CFlyThreadUpdaterInfo*)p_param;
	size_t l_count_insert; // TODO - не юзается
	Lock l(l_p->m_db->m_cs_sqlite);
	sqlite3_transaction l_trans(l_p->m_db->m_flySQLiteDB);
	if (l_p->m_file_full)
		l_p->m_db->process_sql_add_new_fileL(*l_p->m_file_full, l_count_insert);
	if (l_p->m_file_only_counter)
		l_p->m_db->process_sql_add_new_fileL(*l_p->m_file_only_counter, l_count_insert);
	// Создадим новые файлы
	l_p->m_db->internal_process_sql_add_new_fileL();
	l_trans.commit();
	delete l_p;
	return NULL;
}
//========================================================================================================
void CDBManager::flush_inc_counter_fly_file()
{
	{
		const sqlite_int64 l_tick = get_tick_count();
		std::unordered_map<sqlite_int64, unsigned> l_count_query;
		{
			Lock l(m_cs_inc_counter_file);
			l_count_query.swap(m_count_query_cache);
		}
		if (!l_count_query.empty())
		{
			// Инкрементируем счетчики доступа
			sqlite3_transaction l_trans(m_flySQLiteDB);
            unsigned l_cnt = 0;
			for (std::unordered_map<sqlite_int64, unsigned>::const_iterator i = l_count_query.begin(); i != l_count_query.end(); ++i)
			{
				m_update_inc_count_query.init(m_flySQLiteDB,
						"update fly_file set count_query=count_query+?"
#ifdef FLY_SERVER_USE_LAST_DATE_FIELD
						",last_date=strftime('%s','now','localtime')"
#endif
						" where id=?");
				sqlite3_command* l_sql = m_update_inc_count_query.get_sql();
				l_sql->bind(1, i->second);
				l_sql->bind(2, i->first);
				l_sql->executenonquery();
                if (l_cnt++ % 50 == 0)
                    std::cout << '|' << std::flush;
			}
			l_trans.commit();
			const auto l_delta = get_tick_count() - l_tick;
			std::cout << std::endl << "Flush inc counter file: " << l_count_query.size() << " time: " << l_delta << std::endl;
#ifndef _WIN32
			syslog(LOG_NOTICE, "Flush inc counter file: %u time: %u", unsigned(l_count_query.size()), unsigned(l_delta));
#endif
		}
	}
	{
		const sqlite_int64 l_tick = get_tick_count();
		std::map<CFlyFileKey, unsigned> l_download_file_stat_array;
		{
			Lock l(m_cs_inc_download_file);
			l_download_file_stat_array.swap(m_download_file_stat_array);
		}
		if (!l_download_file_stat_array.empty())
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			unsigned l_new_file = 0;
			unsigned l_update_file = 0;
            unsigned l_cnt = 0;
            for (auto i = l_download_file_stat_array.begin(); i != l_download_file_stat_array.end(); ++i)
			{
					m_update_download_count.init(m_flySQLiteDB,
							"update fly_file set count_download=count_download+?"
#ifdef FLY_SERVER_USE_LAST_DATE_FIELD
							",last_date=strftime('%s','now','localtime')"
#endif
							" where tth=? and file_size=?");
				sqlite3_command* l_sql = m_update_download_count.get_sql();
				l_sql->bind(1, i->second);
				l_sql->bind(2, i->first.m_tth,SQLITE_STATIC);
				l_sql->bind(3, i->first.m_file_size);
				l_sql->executenonquery();
				if (m_flySQLiteDB.sqlite3_changes() == 0) // Не было ниразу в поиске?
				{
					m_insert_fly_file_download.init(m_flySQLiteDB,
							"insert or replace into fly_file (tth,file_size,first_date,count_query,count_download) values(?,?,strftime('%s','now','localtime'),0,?)");
					// count_query = 0 обязательно
					sqlite3_command* l_sql = m_insert_fly_file_download.get_sql();
					l_sql->bind(1, i->first.m_tth, SQLITE_STATIC);
					l_sql->bind(2, i->first.m_file_size);
					l_sql->bind(3, i->second);
					l_sql->executenonquery();
					l_new_file++;
				}
				else
				{
					l_update_file++;
				}
                if (l_cnt++ % 100 == 0)
                    std::cout << '|' << std::flush;
			}
			l_trans.commit();
			const auto l_delta = get_tick_count() - l_tick;
			std::cout << std::endl << "Flush download counter file: count " << l_download_file_stat_array.size()
				<< " new:" << l_new_file << " update:" << l_update_file << " time: " << l_delta << std::endl;
#ifndef _WIN32
			syslog(LOG_NOTICE, "Flush download counter file: %u new: %u update: %u time: %u", unsigned(l_download_file_stat_array.size()), l_new_file, l_update_file, unsigned(l_delta));
#endif
		}
	}
	// antivirus - TODO - убрать копи-паст

	{
		const sqlite_int64 l_tick = get_tick_count();
		std::map<CFlyFileKey, unsigned> l_antivirus_file_stat_array;
		{
			Lock l(m_cs_inc_antivirus_file);
			l_antivirus_file_stat_array.swap(m_antivirus_file_stat_array);
		}
		if (!l_antivirus_file_stat_array.empty())
		{
			sqlite3_transaction l_trans(m_flySQLiteDB);
			unsigned l_new_file = 0;
			unsigned l_update_file = 0;
            unsigned l_cnt = 0;
			for (auto i = l_antivirus_file_stat_array.begin(); i != l_antivirus_file_stat_array.end(); ++i)
			{
				m_update_antivirus_count.init(m_flySQLiteDB,
						"update fly_file set count_antivirus=count_antivirus+?"
#ifdef FLY_SERVER_USE_LAST_DATE_FIELD
						",last_date=strftime('%s','now','localtime')"
#endif
						" where tth=? and file_size=?");
				sqlite3_command* l_sql = m_update_antivirus_count.get_sql();
				l_sql->bind(1, i->second);
				l_sql->bind(2, i->first.m_tth, SQLITE_STATIC);
				l_sql->bind(3, i->first.m_file_size);
				l_sql->executenonquery();
				if (m_flySQLiteDB.sqlite3_changes() == 0) // Не было ниразу в поиске?
				{
					m_insert_fly_file_antivirus.init(m_flySQLiteDB,
							"insert or replace into fly_file (tth,file_size,first_date,count_query,count_antivirus) values(?,?,strftime('%s','now','localtime'),0,?)");
					// count_query = 0 обязательно
					sqlite3_command* l_sql = m_insert_fly_file_antivirus.get_sql();
					l_sql->bind(1, i->first.m_tth, SQLITE_STATIC);
					l_sql->bind(2, i->first.m_file_size);
					l_sql->bind(3, i->second);
					l_sql->executenonquery();
					l_new_file++;
				}
				else
				{
					l_update_file++;
				}
                if (l_cnt++ % 100 == 0)
                    std::cout << '|' << std::flush;
			}
			l_trans.commit();
			const auto l_delta = get_tick_count() - l_tick;
			std::cout << std::endl << "Flush antivirus counter file: count " << l_antivirus_file_stat_array.size()
				<< " new:" << l_new_file << " update:" << l_update_file << " time: " << l_delta << std::endl;
#ifndef _WIN32
			syslog(LOG_NOTICE, "Flush antivirus counter file: %u new: %u update: %u time: %u", unsigned(l_antivirus_file_stat_array.size()), l_new_file, l_update_file, unsigned(l_delta));
#endif
		}
	}
}
//========================================================================================================
static void translateDuration(const string& p_audio,
                              string& p_column_audio,
                              string& p_column_duration)  // Отрезалка длительности из поля Audio
{
	p_column_duration.clear();
	p_column_audio.clear();
	if (!p_audio.empty())
	{
		const size_t l_pos = p_audio.find('|', 0);
		if (l_pos != string::npos && l_pos)
		{
			if (p_audio.size() > 6 && p_audio[0] >= '1' && p_audio[0] <= '9' && // Проверим факт наличия в начале длительности
			        (p_audio[1] == 'm' || p_audio[2] == 'n') || // "1mn XXs"
			        (p_audio[1] == 's' || p_audio[2] == ' ') || // "1s XXXms"
			        (p_audio[2] == 's' || p_audio[3] == ' ') || // "59s XXXms"
			        (p_audio[2] == 'm' || p_audio[3] == 'n') ||   // "59mn XXs"
			        (p_audio[1] == 'h') ||  // "1h XXmn"
			        (p_audio[2] == 'h')     // "60h XXmn"
			   )
			{
				p_column_duration = p_audio.substr(0, l_pos - 1);
				if (l_pos + 2 < p_audio.length())
					p_column_audio = p_audio.substr(l_pos + 2);
			}
			else
			{
				p_column_audio = p_audio; // Если не распарсили - показывает что есть
				std::cout << p_audio << std::endl;
				//dcassert(0); // fix "1 076 Kbps,23mn 9s | MPEG Audio, 160 Kbps, 2 channels"
			}
		}
	}
}
//========================================================================================================
void CDBManager::pragma_executor(const char* p_pragma)
{
	static const char* l_db_name[] =
	{
		"main"
		, "fly_db_stats"
        , "fly_db_json"
	};
	for (int i = 0; i < sizeof(l_db_name) / sizeof(l_db_name[0]); ++i)
	{
		string l_sql = "pragma ";
		l_sql += l_db_name[i];
		l_sql += '.';
		l_sql += p_pragma;
		l_sql += ';';
		m_flySQLiteDB.executenonquery(l_sql);
	}
}
//========================================================================================================
bool CDBManager::safeAlter(const char* p_sql)
{
	try
	{
		m_flySQLiteDB.executenonquery(p_sql);
		return true;
	}
	catch (const database_error& e)
	{
		if (e.getError().find("duplicate column name") == string::npos)
		{
			printf("safeAlter: %s", e.getError().c_str());
#ifndef _WIN32
			syslog(LOG_NOTICE, "CDBManager::safeAlter = %s", e.getError().c_str());
#endif
		}
	}
	return false;
}
//========================================================================================================
void CDBManager::errorDB(const string& p_txt, bool p_is_exit_process/* = false */)
{
#ifndef _WIN32
	syslog(LOG_NOTICE, "CDBManager::errorDB = %s", p_txt.c_str());
#endif
	printf("[sql] errorDB - %s\r\nSQL =\r\n%s\r\n=======================\r\n", p_txt.c_str(), g_last_sql.c_str());
	dcdebug("[sql] errorDB - %s\r\nSQL =\r\n%s\r\n=======================\r\n", p_txt.c_str(), g_last_sql.c_str()); // Всегда логируем в файл (т.к. база может быть битой)
	if (p_is_exit_process)
	{
		exit(-1);
	}
}
//========================================================================================================
static string toHexEscape(char val)
{
	char buf[sizeof(int) * 2 + 1 + 1];
	snprintf(buf, sizeof(buf), "%%%X", val & 0x0FF);
	return buf;
}
static char fromHexEscape(const string &aString)
{
	unsigned int res = 0;
	if (sscanf(aString.c_str(), "%X", &res) == EOF)
	{
		// TODO log error!
	}
	return static_cast<char>(res);
}
//========================================================================================================
string CDBManager::encodeURI(const string& aString, bool reverse /* = false*/)
{
	// reference: rfc2396
	string tmp = aString;
	if (reverse)
	{
		// TODO idna: convert host name from punycode
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp.length() > idx + 2 && tmp[idx] == '%' && isxdigit(tmp[idx + 1]) && isxdigit(tmp[idx + 2]))
			{
				tmp[idx] = fromHexEscape(tmp.substr(idx + 1, 2));
				tmp.erase(idx + 1, 2);
			}
			else   // reference: rfc1630, magnet-uri draft
			{
				if (tmp[idx] == '+')
					tmp[idx] = ' ';
			}
		}
	}
	else
	{
		static const string disallowed = ";/?:@&=+$," // reserved
		                                 "<>#%\" "    // delimiters
		                                 "{}|\\^[]`"; // unwise
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp[idx] == ' ')
			{
				tmp[idx] = '+';
			}
			else
			{
				if (tmp[idx] <= 0x1F || tmp[idx] >= 0x7f || (disallowed.find_first_of(tmp[idx])) != string::npos)
				{
					tmp.replace(idx, 1, toHexEscape(tmp[idx]));
					idx += 2;
				}
			}
		}
	}
	return tmp;
}
//========================================================================================================
// Массив параметров не пробрасываемых в справочную таблицу (храним в сыром виде рядом с атрибутом)
// TODO - сделать динамически конфигурируемым
static const char* g_ignore_fly_dic_transform[] =
{
	"Duration",
	"Count",
	"Inform",
	"FrameCount",
	"StreamSize",
	"Title", "Title/More",
	"Track", "Track/Position", "Track/Position_Total",
	"Subject", "Performer", "Performer/Url", "Album", "Album/Performer", "Accompaniment",
	"Composer", "Conductor", "Copyright", "Director", "Lyrics", "Movie", "Original/Album", "Original/Lyricist", "Original/Performer",
	"Recorded_Date", "Publisher",
	"OriginalSourceForm/Name", "OriginalSourceForm/DistributedBy", "OriginalSourceForm",
	"Video_Delay", "Video0_Delay", "DisplayAspectRatio", "DisplayAspectRatio_Original",
	"StreamKindID", "StreamCount", "Source_StreamSize_Proportion", "SamplingCount",
	"Source_StreamSize", "Source_Duration", "Source_FrameCount", "Resolution",
	"ReplayGain_Peak", "ReplayGain_Gain", "BitRate", "BitRate_Maximum", "FrameRate_Maximum", "FrameRate_Minimum", "FrameRate",
	"OverallBitRate", "BufferSize", "Bits-(Pixel*Frame)", "PixelAspectRatio", "BitRate_Nominal", "OverallBitRate_Maximum", "OverallBitRate_Minimum"
	"DataSize", "Delay", "Height", "Width", "Width_Original", "Interleave_Duration", "Interleave_Preload", "Interleave_VideoFrames",
	"fly_audio", "fly_video", "fly_audio_br", "fly_xy", // В справочник не эффективно - длительность лежит в теле - 1h 16mn | AC-3, 448 Kbps, 6 channels.
	// TODO - сделать парсинг на входе.
	0
};
//========================================================================================================
bool CDBManager::is_table_exists(const string& p_table_name)
{
	return m_flySQLiteDB.executeint(
	           "select count(*) from sqlite_master where type = 'table' and lower(tbl_name) = '" + p_table_name + "'") != 0;
}
#endif // FLY_SERVER_USE_SQLITE
//========================================================================================================
void CDBManager::init()
{
#ifndef _WIN32
	openlog("fly-server", 0, LOG_USER); // LOG_PID
	syslog(LOG_NOTICE, "CDBManager init");
#endif
#ifdef FLY_SERVER_USE_FLY_DIC
	m_DIC.resize(e_DIC_LAST - 1);
#endif
#ifdef FLY_SERVER_USE_SQLITE		
	try
	{
		m_flySQLiteDB.open("fly-server-db.sqlite");
		m_flySQLiteDB.setbusytimeout(5000); //  5 Сек

        sqlite3_trace(m_flySQLiteDB.get_db(), gf_trace_callback, NULL);

		m_flySQLiteDB.executenonquery("attach database 'fly-server-stats.sqlite' as fly_db_stats");
        m_flySQLiteDB.executenonquery("attach database 'fly-server-tth-json.sqlite' as fly_db_json");
        m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_db_json.fly_tth_json(tth text PRIMARY KEY not null,is_compress int,json text);");

		pragma_executor("page_size=4096");
		pragma_executor("journal_mode=WAL");
		pragma_executor("temp_store=MEMORY");
		pragma_executor("count_changes=OFF");

		extern int  g_sqlite_cache_db;
		const string l_pragma_cache =  toString(g_sqlite_cache_db * 1024 * 1024 / 4096);
		
		m_flySQLiteDB.executenonquery("PRAGMA cache_size=" + l_pragma_cache);
		m_flySQLiteDB.executenonquery("PRAGMA auto_vacuum=FULL");
		
#ifdef FLY_SERVER_USE_STORE_LOST_LOCATION
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_db_stats.fly_location_ip_lost(ip text PRIMARY KEY not null,last_date text not null);");
#endif
#ifdef FLY_SERVER_USE_LOGIN
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_db_stats.fly_login(id integer PRIMARY KEY AUTOINCREMENT NOT NULL,cid char(39) NOT NULL,ip text NOT NULL,start_date int64 not null,stop_date int64,pid char(39));");
		safeAlter("ALTER TABLE fly_db_stats.fly_login add column pid char(39)");
#endif
		
#ifdef FLY_SERVER_USE_FLY_DIC
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_dic("
		                              "id integer PRIMARY KEY AUTOINCREMENT NOT NULL,dic integer NOT NULL, name text);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_dic_name ON fly_dic(name,dic);");
		// Резервируем запись-пустышку для фиктивной ссылки (OUTER JOIN почему-то не пашет. да и медленней он будет)
		// TODO - отказаться от джойна в пользу подзапросов
		m_flySQLiteDB.executenonquery("insert or replace into fly_dic (id,dic,name) values(0,2,null)");
#endif
		m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_file ("
		                              "id             integer primary key AUTOINCREMENT not null,"
		                              "tth            char(39) not null,"
		                              "file_size      NUMBER not null,"
		                              "count_plus     NUMBER default 0 not null,"
		                              "count_minus    NUMBER default 0 not null,"
		                              "count_fake     NUMBER default 0 not null,"
		                              "count_download NUMBER default 0 not null,"
		                              "count_upload   NUMBER default 0 not null,"
		                              "count_query    NUMBER default 1 not null,"
		                              "first_date     int64 not null,"
		                              "last_date      int64,"
		                              "fly_audio      text,"
		                              "fly_video      text,"
		                              "fly_audio_br   text,"
		                              "fly_xy         text,"
		                              "count_media    NUMBER,"
									  "count_antivirus NUMBER default 0 not null"
		                              ");");
		safeAlter("ALTER TABLE fly_file add column count_media NUMBER");
		safeAlter("ALTER TABLE fly_file add column count_antivirus NUMBER default 0 not null");
		
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_file_tth ON fly_file(TTH,FILE_SIZE);");
		/*      m_flySQLiteDB.executenonquery("CREATE TABLE IF NOT EXISTS fly_mediainfo ("
		                                      " fly_file_id NUMBER not null,"
		                                      " stream_type NUMBER not null,"
		                                      " channel NUMBER not null,"
		                                      " dic_attr_id NUMBER not null,"
		                                      " dic_value_id NUMBER not null,"
		                                      " attr_value text"
		#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
		                                      " ,zlib_attr_value blob"
		#endif
		                                      ");"); // Если dic_value_id = 0(ссылка на пустоту)  то заполняется это поле - оптимизация чтобы не засорять справочник высокоселективными значениями
		#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
		        safeAlter("ALTER TABLE fly_mediainfo add column zlib_attr_value blob");
		#endif
		        m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS iu_fly_mediainfo ON fly_mediainfo(fly_file_id,dic_attr_id,stream_type,channel);");
		    */
		
		m_flySQLiteDB.executenonquery(
		    "CREATE TABLE IF NOT EXISTS fly_registry(segment integer not null, key text not null,val_str text, val_number int64,tick_count int not null);");
		m_flySQLiteDB.executenonquery("CREATE UNIQUE INDEX IF NOT EXISTS "
		                              "iu_fly_registry_key ON fly_registry(segment,key);");
		                              
	}
	catch (const database_error& e)
	{
		errorDB("SQLite: CDBManager::CDBManager" + e.getError());
	}
	// Инитим таблицу параметров без справочников
	if (g_exclude_fly_dic_transform.empty())
		for (int i = 0; g_ignore_fly_dic_transform[i]; ++i)
			g_exclude_fly_dic_transform.insert(g_ignore_fly_dic_transform[i]);
#endif // FLY_SERVER_USE_SQLITE
}
//========================================================================================================
#ifdef FLY_SERVER_USE_SQLITE
void CDBManager::process_store_json_ext_info(const string& p_tth, const int64_t p_id_file, const Json::Value& p_cur_item_in)
{
// Сохраним в виде файла - имя = TTH
	string l_json_ext_info;
	for (int i = 0; i < 100; ++ i)
	{
		string l_json_file_name = "log-new-json//" + p_tth;
		if (i)
		{
			l_json_file_name += "-" + toString(i);
		}
		l_json_file_name += ".json";
		std::fstream l_out(l_json_file_name.c_str(), std::ios_base::in);
		if (!l_out)
		{
			l_out.open(l_json_file_name.c_str(), std::ios_base::out);
			l_json_ext_info = p_cur_item_in.toStyledString();
			l_out << l_json_ext_info << std::endl;
			break;
		}
	}
	if (!l_json_ext_info.empty())
	{
        m_insert_tth_json.init(m_flySQLiteDB,
            "insert or replace into fly_db_json.fly_tth_json (tth,is_compress,json) values(?,?,?)"); // TODO - opt selet + update/insert
        sqlite3_command* l_sql = m_insert_tth_json.get_sql();
        l_sql->bind(1, p_tth, SQLITE_STATIC);
        const auto l_gzip_json = Gzip::compress(l_json_ext_info);
        l_sql->bind(1, p_tth, SQLITE_STATIC);
        if (!l_gzip_json.empty())
        {
            l_sql->bind(2, 1);
            l_sql->bind(3, l_gzip_json, SQLITE_STATIC);
        }
        else
        {
            l_sql->bind(2, 0);
            l_sql->bind(3, l_json_ext_info, SQLITE_STATIC);
        }
        l_sql->executenonquery();
	}
	{
		const Json::Value& l_mediaInfo  = p_cur_item_in["media"];
		CFlyBaseMediainfo l_media;
		l_media.m_audio                 = l_mediaInfo["fly_audio"].asString();
		l_media.m_video                 = l_mediaInfo["fly_video"].asString();
		l_media.m_xy                    = l_mediaInfo["fly_xy"].asString();
		l_media.m_audio_br              = l_mediaInfo["fly_audio_br"].asString();
		if (!l_media.m_audio.empty() || !l_media.m_video.empty() || !l_media.m_audio_br.empty() || !l_media.m_xy.empty())
		{
			set_base_media_attr(p_id_file, l_media, l_json_ext_info.size());
		}
	}
	/*
	  //==== Дополнительный блок медиаинфы
	    const Json::Value& l_attrs_media_ext = p_cur_item_in["media-ext"];
	    const Json::Value& l_attrs_general   = l_attrs_media_ext["general"];
	    const Json::Value::Members& l_members_general = l_attrs_general.getMemberNames();
	    for (int j = 0; j < l_members_general.size(); ++j)
	    {
	        // const std::string l_general_key = l_members_general[j];
	        // const std::string l_general_value = l_attrs_general[l_members_general[j]].asString();
	        const CFlyMediaAttrItem l_attr_pair(l_members_general[j], l_attrs_general[l_members_general[j]].asString(), 0, 0);
	        l_attr_array.push_back(l_attr_pair);
	    }
	    const Json::Value& l_attrs_video = l_attrs_media_ext["video"];
	    const Json::Value::Members& l_members_video = l_attrs_video.getMemberNames();
	    for (int j = 0; j < l_members_video.size(); ++j)
	    {
	        const CFlyMediaAttrItem l_attr_pair(l_members_video[j], l_attrs_video[l_members_video[j]].asString(), 1, 0);
	        l_attr_array.push_back(l_attr_pair);
	    }
	    const Json::Value& l_attrs_audio = l_attrs_media_ext["audio"];
	    const Json::Value::Members& l_members_audio = l_attrs_audio.getMemberNames();
	    const char* l_channel_mask = "channel-";
	    if (l_members_audio.size() >= 1 && l_members_audio[0].compare(0, 8, l_channel_mask) == 0)
	    {
	        for (int k = 0; k < l_members_audio.size(); ++k)   // TODO - поддержать возможный вариант с channel-all
	            // требуется отладка на файлах.
	        {
	            if (l_members_audio[k].compare(0, 8, l_channel_mask) == 0) // TODO после отладки можно убрать сравнение для каждого канала?
	            {
	                const Json::Value& l_attrs_audio_channel = l_attrs_audio[l_members_audio[k]];
	                // Ускоренный детект канала - all
	                int l_id_channel;
	                if (l_members_audio[k].length() == 11 &&
	                        l_members_audio[k][8]  == 'a' &&
	                        l_members_audio[k][9]  == 'l' &&
	                        l_members_audio[k][10] == 'l') // "channel-all"
	                    l_id_channel = 255;
	                else
	                    l_id_channel = atoi(l_members_audio[k].c_str() + 8);
	
	#ifdef _DEBUG
	//                      std::cout << "l_members_audio[k] = " << l_members_audio[k] << std::endl;
	#endif
	                const Json::Value::Members& l_members_audio_channel = l_attrs_audio_channel.getMemberNames();
	                for (int z = 0; z < l_members_audio_channel.size(); ++z)
	                {
	                    const CFlyMediaAttrItem l_attr_pair(l_members_audio_channel[z],
	                                                        l_attrs_audio_channel[l_members_audio_channel[z]].asString(),
	                                                        2,
	                                                        l_id_channel);
	                    l_attr_array.push_back(l_attr_pair);
	#ifdef _DEBUG
	//                      std::cout << "l_members_audio_channel[z] z = " << z << std::endl << " l_attrs_audio_channel[l_members_audio_channel[z]].asString() = " << l_attrs_audio_channel[l_members_audio_channel[z]].asString() << std::endl;
	#endif
	                }
	            }
	            else
	            {
	                std::cout << "Error mask channel-* =  l_members_audio[" << k << "] = " << l_members_audio[k] << std::endl;
	#ifndef _WIN32
	                syslog(LOG_NOTICE, "Error mask channel-* =  l_members_audio[%d] = %s", k , l_members_audio[k].c_str());
	#endif
	            }
	        }
	    }
	    else
	    {
	        // Отработаем без каналов
	        for (int j = 0; j < l_members_audio.size(); ++j)
	        {
	            const CFlyMediaAttrItem l_attr_pair(l_members_audio[j], l_attrs_audio[l_members_audio[j]].asString(), 2, 0);
	            l_attr_array.push_back(l_attr_pair);
	        }
	    }
	    if (!l_attr_array.empty())
	    {
	        set_attr_array(p_id_file, l_attr_array);
	  }
	*/
	
}
//========================================================================================================
std::string CDBManager::get_ext_gzip_json(const std::string& p_tth)
{
    string l_json_ext_info;
    sqlite3_command* l_sql = m_select_tth_json.init(m_flySQLiteDB,
        "select json,is_compress from fly_db_json.fly_tth_json where tth=?");
    l_sql->bind(1, p_tth, SQLITE_STATIC);
    sqlite3_reader l_json = l_sql->executereader();
    if (l_json.read())
    {
        const auto l_json_gzip = l_json.getstring(0);
        const auto l_is_gzip = l_json.getint(1);
        if (l_is_gzip)
        {
            l_json_ext_info = Gzip::decompress(l_json_gzip);
        }
        else
        {
            l_json_ext_info = l_json_gzip;
        }
    }
    return l_json_ext_info;
}

//========================================================================================================
void CDBManager::process_get_query(const size_t p_index_result,
                                   const string& p_tth,
                                   const string& p_size_str,
                                   const int64_t p_id_file,
                                   const Json::Value& p_cur_item_in,
                                   Json::Value& p_result_arrays,
                                   const Json::Value& p_result_counter,
                                   bool p_is_all_only_ext_info,
                                   bool p_is_different_ext_info,
                                   bool p_is_all_only_counter,
                                   bool p_is_different_counter,
                                   bool p_is_only_counter,
                                   const CFlyFileRecord* p_file_record,
                                   size_t& p_only_ext_info_counter)
{
	bool l_is_only_ext_info = p_is_all_only_ext_info;
	if (p_is_different_ext_info == true &&  l_is_only_ext_info == false)
		l_is_only_ext_info = p_cur_item_in["only_ext_info"].asInt() == 1; // Расширенная инфа нужна для этого элемента?
	if (l_is_only_ext_info)
		++p_only_ext_info_counter;
	//  CFlyMediaAttrArray l_attr_array;
	Json::Value& l_cur_item_out = p_result_arrays[int(p_index_result)];
	Json::Value l_base_mediainfo;
	
	if (p_is_only_counter == false) // Если запросили только счетчики для группы или всех, то не обращаемся к базе атрибутов
	{
		//dcassert(l_is_only_ext_info == false && p_file_record->m_audio.empty());
		if (p_file_record && l_is_only_ext_info == false)
		{
			// Есть уже ранее загруженная запись
			
			if (!p_file_record->m_media.m_audio.empty())
				l_base_mediainfo["fly_audio"] = p_file_record->m_media.m_audio;
			if (!p_file_record->m_media.m_audio_br.empty())
				l_base_mediainfo["fly_audio_br"] = p_file_record->m_media.m_audio_br;
			if (!p_file_record->m_media.m_video.empty())
				l_base_mediainfo["fly_video"] = p_file_record->m_media.m_video;
			if (!p_file_record->m_media.m_xy.empty())
				l_base_mediainfo["fly_xy"] = p_file_record->m_media.m_xy;
		}
		else
		{
			if (l_is_only_ext_info)
			{
                const string l_json_ext_info = get_ext_gzip_json(p_tth);
				if (!l_json_ext_info.empty())
				{
					Json::Value l_root;
					Json::Reader reader(Json::Features::strictMode());
					const bool parsingSuccessful = reader.parse(l_json_ext_info, l_root);
					if (!parsingSuccessful)
					{
						std::cout  << "Failed to parse json configuration: l_json -> fly-server-error.log" << std::endl;
					}
					else
					{
						l_cur_item_out = l_root; // TODO - лишний
					}
				}
				
#ifndef _WIN32 // TODO debug
				syslog(LOG_NOTICE, "get_ext_media_attr_array (Inform) = %u", (unsigned)p_id_file);
#else
				std::cout << "get_ext_media_attr_array (Inform) p_id_file = " << p_id_file << std::endl;
#endif
			}
		}
	}
	
	l_cur_item_out["info"]  = p_result_counter;
	l_cur_item_out["tth"]   = p_tth;
	l_cur_item_out["size"]  = p_size_str;
	if (!l_base_mediainfo.empty())
	{
		l_cur_item_out["media"] = l_base_mediainfo;
	}
}
//========================================================================================================
std::string CDBManager::store_media_info(CFlyServerContext& p_flyserver_cntx)
{
	string l_res_stat;
	try
	{
		const bool l_is_get = p_flyserver_cntx.m_query_type == FLY_POST_QUERY_GET;
		const bool l_is_set = p_flyserver_cntx.m_query_type == FLY_POST_QUERY_SET;
		if (!(l_is_get || l_is_set
		        //  || p_query == "fly-inc-count-minus" ||
		        //p_query == "fly-inc-count-fake" ||
		        //p_query == "fly-inc-count-download" ||
		        //p_query == "fly-inc-count-upload"
		     ))
		{
			std::cout << "Error find metod fly-* or fly-get / fly-set" << std::endl;
		}
		Json::Value l_root;
		Json::Reader reader(Json::Features::strictMode());
		const std::string l_json = l_is_get ? p_flyserver_cntx.m_in_query : encodeURI(p_flyserver_cntx.m_in_query, true); // Если get то декодинг не нужен - все буквы латиницей
		const bool parsingSuccessful = reader.parse(l_json, l_root);
		if (!parsingSuccessful)
		{
			std::cout  << "Failed to parse json configuration: l_json -> fly-server-error.log" << std::endl;
			std::fstream l_out("fly-server-error.log", std::ios_base::out);
			l_out << l_json << std::endl;
			return "Failed to parse json configuration";
		}
		else
		{
#ifdef _DEBUG
			std::cout << "[OK - in] ";
			//std::fstream l_out("fly-server-OK-in-debug.log", std::ios_base::out);
			//l_out << l_root;
#endif
		}
		// Входной JSON-запрос
		// Распарсим общие значения
		bool l_is_all_only_ext_info  =  false;
		bool l_is_different_ext_info =  false;
		bool l_is_different_counter  =  false;
		p_flyserver_cntx.m_count_cache = 0;
		const bool l_is_all_only_counter   =  l_root["only_counter"].asInt() == 1; // Для всего блока нужны только счетчики?
		if (l_is_all_only_counter == false)
		{
			l_is_different_ext_info =  l_root["different_ext_info"].asInt() == 1; // В запросе есть желание получить и полную и не полную инфу
			// - при формировании выборки провести анализ признака only_ext_info для каждого файла.
			if (l_is_different_ext_info == false)
			{
				l_is_all_only_ext_info  =  l_root["only_ext_info"].asInt() == 1; // Расширенная инфа не нужна для всего блока?
				if (l_is_all_only_ext_info == false)
				{
					l_is_different_counter =  l_root["different_counter"].asInt() == 1; // Счетчики нужны только некоторым записям?
					if (l_is_different_counter)
						p_flyserver_cntx.m_count_cache = l_root["cache"].asInt();
				}
			}
		}
		// Обходим массив
		const Json::Value& l_array = l_root["array"];
		std::unique_ptr<CFlyFileRecordMap> l_sql_result_array(new CFlyFileRecordMap);
		std::unique_ptr<CFlyFileRecordMap> l_sql_result_array_only_counter(new CFlyFileRecordMap);
		prepare_and_find_all_tth(
		    l_root,
		    l_array,
		    *l_sql_result_array, // Результат запросов со счетчиком медиаинфы
		    *l_sql_result_array_only_counter, // Результат запросов только со счетчиком рейтингов
		    l_is_all_only_counter,
		    l_is_all_only_ext_info,
		    l_is_different_ext_info,
		    l_is_different_counter
		);
		p_flyserver_cntx.m_count_get_only_counter = l_sql_result_array_only_counter->size();
		p_flyserver_cntx.m_count_get_base_media_counter = l_sql_result_array->size();
		p_flyserver_cntx.m_count_file_in_json = l_array.size();
		Json::Value  l_result_root; // Выходной JSON-пакет
		Json::Value& l_result_arrays = l_result_root["array"];
		size_t l_index_result = 0;
		for (int j = 0; j < p_flyserver_cntx.m_count_file_in_json; ++j)
		{
			const Json::Value& l_cur_item_in = l_array[j];
			bool l_is_only_counter = l_is_all_only_counter;
			if (l_is_get)
			{
				if (l_is_different_counter == true && l_is_only_counter == false)
					l_is_only_counter = l_cur_item_in["only_counter"].asInt() == 1; // Только счетчики нужны для этого элемента?
			}
			const string l_size_str = l_cur_item_in["size"].asString();
			int64_t l_size = 0;
			if (!l_size_str.empty())
				l_size = _atoi64(l_size_str.c_str());
			else
			{
#ifndef _WIN32
				syslog(LOG_NOTICE, "l_cur_item_in[size] is null!");
#endif
			}
			const string l_tth = l_cur_item_in["tth"].asString();
			
			// Попытка найти данные в массиве - уменьшаем кол-во SQL выборок для запроса по чтению
			const CFlyFileRecord* l_cur_record = NULL;
			if (l_is_get)
			{
				const CFlyFileKey l_tth_key(l_tth, l_size);
				CFlyFileRecordMap::const_iterator l_find_mediainfo = l_sql_result_array->find(l_tth_key);
				if (l_find_mediainfo != l_sql_result_array->end()) // Найдем инфу сначала в массиве с медиаинфой
				{
					l_cur_record = &l_find_mediainfo->second;
				}
				else
				{
					CFlyFileRecordMap::const_iterator l_find_counter = l_sql_result_array_only_counter->find(l_tth_key); // Если не нашли, поищем по каунтерам
					if (l_find_counter != l_sql_result_array_only_counter->end())
					{
						l_cur_record = &l_find_counter->second;
					}
				}
			}
			Json::Value  l_result_counter;
			int64_t l_count_query = 0;
			int64_t l_count_download = 0;
			int64_t l_count_antivirus = 0;
			int64_t l_id_file     = 0;
			if (l_cur_record && l_cur_record->m_fly_file_id) // Нашли информацию массовым запросом - без дополнительного SQL формируем ответ?
			{
				l_id_file     = l_cur_record->m_fly_file_id;
				l_count_query = l_cur_record->m_count_query;
				l_count_download = l_cur_record->m_count_download;
				l_count_antivirus = l_cur_record->m_count_antivirus;
				dcassert(l_id_file);
				if (l_cur_record->m_count_download)
					l_result_counter["count_download"] = toString(l_cur_record->m_count_download);
				if (l_cur_record->m_count_antivirus)
					l_result_counter["count_antivirus"] = toString(l_cur_record->m_count_antivirus);
				if (l_cur_record->m_count_query)
					l_result_counter["count_query"] = toString(l_cur_record->m_count_query);
				if (l_cur_record->m_count_mediainfo)
					l_result_counter["count_media"] = toString(l_cur_record->m_count_mediainfo);
				if (l_cur_record->m_is_new_file == true) // Если файл новый скинем ему счетчик (чтобы не шел update +1)
					l_count_query = 0;
			}
			else
			{
				l_cur_record = NULL;
				// Если не нашли в кэше - обращаемся старым вариантом к базе данных
				l_id_file = find_tth(l_tth,
				                     l_size,
									 l_is_set,
				                     l_result_counter,
				                     l_is_get,
				                     (l_is_only_counter == true && l_is_set == false) ? true : false,
				                     l_count_query,
				                     l_count_download,
									 l_count_antivirus);
			}
			// Если запроc типа get + заказывают "только счетчики", то мы находимся в файл-листе
			// Дополнительно расчитаем сколько данных у нас лежит в медиаинфе и вернем назад на клиент для анализа
			// если будет возможность клиент нам докинет медиаинформацию для пополнения базы
			if (l_is_get && l_id_file && l_count_query) // Если файл уже был сохраним его ID для массового апдейта в одной траназакции
			{
				Lock l(m_cs_inc_counter_file);
				m_count_query_cache[l_id_file]++;
			}
			if (l_is_set)
			{
				dcassert(l_id_file);
				process_store_json_ext_info(l_tth, l_id_file, l_cur_item_in); // TODO - в нитку
			}
			else if (l_is_get)
			{
				process_get_query(l_index_result++, // TODO оптимизировать при случаях если l_cur_record найден.
				                  l_tth,
				                  l_size_str,
				                  l_id_file,
				                  l_cur_item_in,
				                  l_result_arrays,
				                  l_result_counter,
				                  l_is_all_only_ext_info,
				                  l_is_different_ext_info,
				                  l_is_all_only_counter,
				                  l_is_different_counter,
				                  l_is_only_counter,
				                  l_cur_record,
				                  p_flyserver_cntx.m_count_get_ext_media_counter);
			}
		}
		if (l_is_get)
		{
			if (l_index_result) // Генерим если есть ответ из базы
			{
				l_res_stat = l_result_root.toStyledString();
			}
			else
			{
#ifdef _DEBUG
				std::cout << "[l_result_root.empty()]" << std::endl;
#endif
			}
		}
		// Финализируеся
		// 1. Инкрементируем счетчики доступа
		// 2. Элементы не найденные в мапе вставляем в базу данных с каунтером = 1
		// Признаком нового файла является m_fly_file_id = 0;
		const bool l_is_new_file_only_counter = l_sql_result_array_only_counter->is_new_file();
		const bool l_is_new_file = l_sql_result_array->is_new_file();
		if (l_is_new_file_only_counter || l_is_new_file)
		{
			CFlyThreadUpdaterInfo* l_thread_param = new CFlyThreadUpdaterInfo(this);
			if (l_is_new_file)
				l_thread_param->m_file_full = l_sql_result_array.release();
			if (l_is_new_file_only_counter)
				l_thread_param->m_file_only_counter = l_sql_result_array_only_counter.release();
			if (mg_start_thread(thread_proc_sql_add_new_file, l_thread_param)) // TODO - поток обединить с thread_proc_inc_counter_thread
			{
				delete l_thread_param;
			}
		}
		{
			static int g_count = 1;
			if (++g_count % 100 == 0)
			{
				if (mg_start_thread(thread_proc_sql_inc_counter_file, this))
				{
					//
				}
			}
		}
	}
	catch (const database_error& e)
	{
		p_flyserver_cntx.m_error = e.getError();
		errorDB("[sqlite] store_media_info error: " + e.getError(), false);
	}
	return l_res_stat;
}
//========================================================================================================
#ifdef FLY_SERVER_USE_FLY_DIC
int64_t CDBManager::calc_dic_value_id(const string& p_param, const string& p_value, int64_t& p_dic_attr_id, bool p_create)
{
	p_dic_attr_id  = get_dic_id(p_param, e_DIC_MEDIA_ATTR_TYPE, p_create);
	int64_t l_dic_value_id;
	if (g_exclude_fly_dic_transform.find(p_param) == g_exclude_fly_dic_transform.end()) // Запись  нужно прокидывать в справочник?
		l_dic_value_id = get_dic_id(p_value, e_DIC_MEDIA_ATTR_VALUE, p_create); // Получим код значения в справочнике. если нет значения - создадим.
	else
		l_dic_value_id = 0;
	return l_dic_value_id;
}
#endif
//========================================================================================================
void CDBManager::set_base_media_attr(int64_t p_file_id, const CFlyBaseMediainfo& p_media, int p_count_media)
{
	Lock l(m_cs_sqlite);
	try
	{
		m_insert_base_attr.init(m_flySQLiteDB,
			  "update fly_file set fly_audio=?,fly_audio_br=?,fly_video=?,fly_xy=?,count_media=? where id=?"); // TODO унести в общий апдейт или нет?
		sqlite3_command* l_sql = m_insert_base_attr.get_sql();
		l_sql->bind(1, p_media.m_audio, SQLITE_STATIC);
		l_sql->bind(2, p_media.m_audio_br, SQLITE_STATIC);
		l_sql->bind(3, p_media.m_video, SQLITE_STATIC);
		l_sql->bind(4, p_media.m_xy, SQLITE_STATIC);
		l_sql->bind(5, p_count_media);
		l_sql->bind(6, (long long)(p_file_id));
		l_sql->executenonquery();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - set_base_media_attr: " + e.getError());
	}
}
//========================================================================================================
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
void CDBManager::zlib_convert_attr_value()
{
	Lock l(m_cs_sqlite);
	try
	{
		std::cout << "CDBManager::zlib_convert_attr_value!" << std::endl;
		if (!m_zlib_convert_select.get())
			m_zlib_convert_select = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                      "SELECT attr_value,rowid FROM fly_mediainfo where length(attr_value) > 150")); // fly_file_id,dic_attr_id,stream_type,channel
			                                                                      
		sqlite3_reader l_q = m_zlib_convert_select->executereader();
		int l_count_convert = 0;
		while (l_q.read())
		{
			const string l_attr  = l_q.getstring(0);
			int l_zlib_result;
			std::vector<uint8_t> l_dest_data;
			if (zlib_compress(l_attr.c_str(), l_attr.size(), l_dest_data, l_zlib_result, 9))
			{
				if (!m_zlib_convert_update.get())
					m_zlib_convert_update = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                      "update fly_mediainfo set zlib_attr_value=?,attr_value=null where rowid = ?"));
				const int64_t l_rowid = l_q.getint64(1);
				m_zlib_convert_update->bind(1, l_dest_data.data(), l_dest_data.size(), SQLITE_TRANSIENT);
				m_zlib_convert_update->bind(2, l_rowid);
				m_zlib_convert_update->executenonquery();
				++l_count_convert;
				if ((l_count_convert % 1000) == 0)
				{
					std::cout << "zlib - count_convert " << l_count_convert << std::endl;
				}
			}
		}
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - zlib_convert_attr_value: " + e.getError());
	}
}
#endif
#if 0
void CDBManager::set_attr_array(int64_t p_file_id, CFlyMediaAttrArray& p_array)
{
	Lock l(m_cs_sqlite);
	try
	{
		// Проходим один раз массив для генерации DIC_ID (т.к. вложенные транзакции не поддерживаются)
		for (CFlyMediaAttrArray::iterator i = p_array.begin(); i != p_array.end(); ++i)
		{
			i->m_dic_value_id = calc_dic_value_id(i->m_param, i->m_value, i->m_dic_attr_id, true);
		}
		
		sqlite3_transaction l_trans(m_flySQLiteDB);
		if (!m_insert_attr_array.get())
			m_insert_attr_array = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                    "insert or replace into fly_mediainfo (fly_file_id,dic_attr_id,stream_type,channel,dic_value_id,attr_value"
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
			                                                                    ",zlib_attr_value"
#endif
			                                                                    ") values(?,?,?,?,?,?"
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
			                                                                    ",?"
#endif
			                                                                    ")"));
		sqlite3_command* l_sql = m_insert_attr_array.get();
		int l_count_duplicate = 0;
		for (CFlyMediaAttrArray::const_iterator i = p_array.begin(); i != p_array.end(); ++i)
		{
			if (i->m_value.empty())
			{
				std::cout << "i->m_param is empty! " << i->m_param << std::endl;
				continue;
			}
			{
				if (!m_select_attr_array.get())
					m_select_attr_array = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                    "select dic_value_id,attr_value"
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
					                                                                    ",zlib_attr_value"
#endif
					                                                                    " from fly_mediainfo where fly_file_id=? and dic_attr_id=? and stream_type=? and channel=?"));
				m_select_attr_array->bind(1, (long long int)p_file_id);
				m_select_attr_array->bind(2, (long long int)i->m_dic_attr_id);
				m_select_attr_array->bind(3, (long long int)i->m_stream_type);
				m_select_attr_array->bind(4, (long long int)i->m_channel);
				sqlite3_reader l_q_check = m_select_attr_array->executereader();
				if (l_q_check.read())
				{
					const int64_t l_cur_dic_value_id = l_q_check.getint64(0);
					string l_attr_value = l_q_check.getstring(1);
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
					std::vector<uint8_t> l_zlib_attr_value;
					if (l_cur_dic_value_id == 0 && l_attr_value.empty()) // В таблице лежит сжатый контент?
					{
						l_q_check.getblob(2, l_zlib_attr_value);
						std::vector<unsigned char> l_decompress;
						if (zlib_uncompress(l_zlib_attr_value.data(), l_zlib_attr_value.size(), l_decompress))
							l_attr_value = (char *)l_decompress.data();
					}
#endif
					if (l_cur_dic_value_id == i->m_dic_value_id && l_attr_value == i->m_value)
					{
						l_count_duplicate++;
						continue;
					}
				}
			}
			if (i->m_dic_attr_id)
			{
				l_sql->bind(1, (long long int)p_file_id);
				l_sql->bind(2, (long long int)i->m_dic_attr_id);
				l_sql->bind(3, (long long int)i->m_stream_type);
				l_sql->bind(4, (long long int)i->m_channel);
				l_sql->bind(5, (long long int)i->m_dic_value_id);
				if (i->m_dic_value_id == 0)
				{
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
					if (i->m_value.size() > 150)
					{
						std::vector<uint8_t> l_dest_data;
						int l_zlib_result;
						zlib_compress(i->m_value.c_str(), i->m_value.size(), l_dest_data, l_zlib_result, 9);
						if (!l_dest_data.empty())
						{
							l_sql->bind(6); // в тексте пустота
							l_sql->bind(7, l_dest_data.data(), l_dest_data.size(), SQLITE_TRANSIENT);
						}
						else
						{
							l_sql->bind(6, i->m_value.c_str(), SQLITE_TRANSIENT); // TODO  SQLITE_STATIC?
							l_sql->bind(7); // Сжатие не используется
						}
					}
					else
#endif
					{
						l_sql->bind(6, i->m_value.c_str(), SQLITE_TRANSIENT); // TODO  SQLITE_STATIC?
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
						l_sql->bind(7); // Сжатие не используется
#endif
					}
				}
				else
				{
					l_sql->bind(6);
#ifdef FLYSERVER_USE_ZLIB_ATTR_VALUE
					l_sql->bind(7);
#endif
				}
				l_sql->executenonquery();
			}
			else
			{
				dcassert(0);
			}
		}
		if (l_count_duplicate)
		{
#ifdef _DEBUG
			std::cout << std::endl << "[!] count dup = " << l_count_duplicate << " count normal = " << p_array.size() << std::endl;
#endif
#ifndef _WIN32
			syslog(LOG_NOTICE, "set_attr_array duplicate [%d/%d] update p_file_id = %lld", l_count_duplicate, p_array.size(), p_file_id);
#endif
		}
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - set_attr_array: " + e.getError());
	}
}
#endif // 0
//========================================================================================================
void CDBManager::antivirus_inc(const string& p_JSON)
{
	Json::Value l_root;
	Json::Reader reader(Json::Features::strictMode());
	const bool parsingSuccessful = reader.parse(p_JSON, l_root);
	if (!parsingSuccessful)
	{
		std::cout  << "Failed to parse json configuration: l_json -> fly-server-antivirus-error.log" << std::endl;
		std::fstream l_out("fly-server-antivirus-error.log", std::ios_base::out);
		l_out << p_JSON << std::endl;
		return;
	}
	else
	{
		Lock l(m_cs_inc_antivirus_file);
		const Json::Value& l_array = l_root["array"];
		size_t l_count_file_in_json = l_array.size();
		for (int j = 0; j < l_count_file_in_json; ++j)
		{
			const Json::Value& l_cur_item_in = l_array[j];
			const CFlyFileKey l_tth_key(l_cur_item_in["tth"].asString(), _atoi64(l_cur_item_in["size"].asString().c_str()));
			m_antivirus_file_stat_array[l_tth_key]++;
		}
		const Json::Value& l_array_tth_file_list = l_root["array_file_list"];
		size_t l_count_file_in_json_file_list = l_array_tth_file_list.size();
		for (int j = 0; j < l_count_file_in_json_file_list; ++j)
		{
			const Json::Value& l_tth = l_array_tth_file_list[j];
			const Json::Value& l_tth_array = l_tth["TTH"];
			size_t l_count_tth = l_tth_array.size();
			for (int k = 0; k < l_count_tth; ++k)
			{
				const Json::Value& l_cur_item_in = l_tth_array[k];
				const CFlyFileKey l_tth_key(l_cur_item_in["tth"].asString(), _atoi64(l_cur_item_in["size"].asString().c_str()));
				m_antivirus_file_stat_array[l_tth_key]++;
			}
		}
	}
}
//========================================================================================================
void CDBManager::add_download_tth(const string& p_json)
{
	Json::Value l_root;
	Json::Reader reader(Json::Features::strictMode());
	const bool parsingSuccessful = reader.parse(p_json, l_root);
	if (!parsingSuccessful)
	{
		std::cout << "Failed to parse json configuration: l_json -> fly-server-download-torrent-error.log" << std::endl;
		std::fstream l_out("fly-server-download0torrent-error.log", std::ios_base::out);
		l_out << p_json << std::endl;
		return;
	}
	else
	{
		const Json::Value& l_array = l_root["array"];
		size_t l_count_file_in_json = l_array.size();
		Lock l(m_cs_inc_download_file);
		for (int j = 0; j < l_count_file_in_json; ++j)
		{
			const Json::Value& l_cur_item_in = l_array[j];
			const CFlyFileKey l_tth_key(l_cur_item_in["tth"].asString(), _atoi64(l_cur_item_in["size"].asString().c_str()));
			m_download_file_stat_array[l_tth_key]++;
		}
	}
}
//========================================================================================================
void CDBManager::add_download_torrent(const string& p_json)
{
	Json::Value l_root;
	Json::Reader reader(Json::Features::strictMode());
	const bool parsingSuccessful = reader.parse(p_json, l_root);
	if (!parsingSuccessful)
	{
		std::cout << "Failed to parse json configuration: l_json -> fly-server-download-error.log" << std::endl;
		std::fstream l_out("fly-server-download-error.log", std::ios_base::out);
		l_out << p_json << std::endl;
		return;
	}
	else
	{
/*
       TODO - обработать торрентовские sha
		const Json::Value& l_array = l_root["array"];
		size_t l_count_file_in_json = l_array.size();
		Lock l(m_cs_inc_download_file);
		for (int j = 0; j < l_count_file_in_json; ++j)
		{
			const Json::Value& l_cur_item_in = l_array[j];
			const CFlyFileKey l_tth_key(l_cur_item_in["tth"].asString(), _atoi64(l_cur_item_in["size"].asString().c_str()));
			m_download_file_stat_array[l_tth_key]++;
		}
*/
	}
}
//========================================================================================================
string CDBManager::login(const string& p_JSON, const string& p_IP)
{
	Json::Value  l_result_root;
	l_result_root["ID"] = "0";
	const string l_json_result = l_result_root.toStyledString();
	return l_json_result;
#ifdef FLY_SERVER_USE_LOGIN
	
	string l_json_result;
	Json::Value l_root;
	Json::Reader reader(Json::Features::strictMode());
	const std::string l_json = encodeURI(p_JSON, true);
	const bool parsingSuccessful = reader.parse(l_json, l_root);
	if (!parsingSuccessful)
	{
		std::cout  << "Failed to parse json configuration: l_json -> fly-server-error.log" << std::endl;
		std::fstream l_out("fly-server-login-error.log", std::ios_base::out);
		l_out << l_json << std::endl;
		return "Failed to parse json configuration";
	}
	else
	{
#ifdef _DEBUG
		std::cout << "[OK - in] ";
		std::fstream l_out("fly-server-login-OK-in-debug.log", std::ios_base::out);
		l_out << l_root;
#endif
		const Json::Value& l_login_info = l_root["login"];
		const string l_CID = l_login_info["CID"].asString();
		const string l_PID = l_login_info["PID"].asString();
		if (!l_CID.empty())
		{
			try
			{
				Lock l(m_cs_sqlite);
				int64_t l_id = 0;
				if (!m_insert_fly_login.get())
					m_insert_fly_login = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					                                                                   "insert into fly_db_stats.fly_login (CID,PID,IP,start_date) values(?,?,?,strftime('%s','now','localtime'))"));
				sqlite3_command* l_sql = m_insert_fly_login.get();
				l_sql->bind(1, l_CID, SQLITE_STATIC);
				if (!l_PID.empty())
					l_sql->bind(2, l_PID, SQLITE_STATIC);
				else
					l_sql->bind(2);
				l_sql->bind(3, p_IP, SQLITE_STATIC);
				l_sql->executenonquery();
				{
#ifdef FLY_SERVER_USE_STORE_LOST_LOCATION
					// Сохраним потерянные IP
					const Json::Value& l_lost_ip = l_login_info["customlocation_lost_ip"];
					const int l_count_lost_ip = l_lost_ip.size();
					if (l_count_lost_ip)
					{
						if (!m_insert_fly_location_ip_lost.get())
							m_insert_fly_location_ip_lost = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
							                                                                              "insert or replace into fly_db_stats.fly_location_ip_lost (ip,last_date) values(?,datetime('now','localtime'))"));
						for (int i = 0; i < l_count_lost_ip; ++i)
						{
							const string& l_ip = l_lost_ip[i].asString();
							sqlite3_command* l_sql = m_insert_fly_location_ip_lost.get();
							l_sql->bind(1, l_ip, SQLITE_STATIC);
							l_sql->executenonquery();
#ifndef _WIN32
							// Не спамим в сислог. syslog(LOG_NOTICE, "customlocation lost ip = %s",l_ip.c_str());
#endif
						}
					}
#endif // FLY_SERVER_USE_STORE_LOST_LOCATION
				}
				l_id = m_flySQLiteDB.insertid();
				Json::Value  l_result_root;
				//Json::Value& l_result_prop = l_result_root["info"];
				l_result_root["ID"] = toString(l_id);
				l_json_result = l_result_root.toStyledString();
			}
			catch (const database_error& e)
			{
				errorDB("SQLite - login: error = " + e.getError());
			}
		}
	}
	return l_json_result;
#endif
}
//========================================================================================================
#if 0
void CDBManager::inc_counter_fly_file_bulkL(const std::vector<sqlite_int64>& p_id_array)
{
    Lock l(m_cs_sql_cache);
	dcassert(p_id_array.size());
	if (p_id_array.size())
	{
#ifdef FLY_SERVER_USE_ARRAY_UPDATE
		sqlite3_command* l_sql_command = NULL;
		CFlyCacheSQLCommandInt& l_pool_sql = m_sql_cache[SQL_CACHE_UPDATE_COUNT];
		CFlyCacheSQLCommandInt::const_iterator l_sql_it = l_pool_sql.find(p_id_array.size());
		if (l_sql_it == l_pool_sql.end())
		{
			string l_sql_text =  "update fly_file set count_query=count_query+1"
#ifdef FLY_SERVER_USE_LAST_DATE_FIELD
				", last_date=strftime('%s','now','localtime')"
#endif
				" where id in(?";
			for (unsigned i = 1; i < p_id_array.size(); ++i)
			{
				l_sql_text += ",?";
			}
			l_sql_text += ")";
			l_sql_command = new sqlite3_command(m_flySQLiteDB, l_sql_text);
			l_pool_sql.insert(std::make_pair(p_id_array.size(), l_sql_command));
		}
		else
			l_sql_command = l_sql_it->second;
		for (unsigned i = 0; i < p_id_array.size(); ++i)
		{
			l_sql_command->bind(i + 1, (long long int)p_id_array[i]);
		}
		l_sql_command->executenonquery();
#else
		
			if (!m_update_inc_count_query.get())
				m_update_inc_count_query = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
					"update fly_file set count_query=count_query+1"
#ifdef FLY_SERVER_USE_LAST_DATE_FIELD
					",last_date=strftime('%s','now','localtime')"
#endif
					" where id=?"));
		sqlite3_command* l_sql = m_update_inc_count_query.get();
		for (unsigned i = 0; i < p_id_array.size(); ++i)
		{
			l_sql->bind(1,p_id_array[i]);
			l_sql->executenonquery();
		}
#endif
	}
}
#endif

//========================================================================================================
// TODO пока не используется.
/*
void CDBManager::inc_counter_fly_file(int64_t p_id, const std::string& p_type)
{
    sqlite3_command* l_sql_command = NULL;
    CFlyCacheSQLCommand::const_iterator l_sql_it = m_inc_counter_fly_file.find(p_type);
    if (l_sql_it == m_inc_counter_fly_file.end())
    {
     char l_buf[200];
     l_buf[0] = 0;
     snprintf(l_buf, sizeof(l_buf), "update fly_file set %s=%s+1, last_date=%s where id=?",
             p_type.c_str(),
             p_type.c_str(),
             "strftime('%s','now','localtime')");
     l_sql_command = new sqlite3_command(m_flySQLiteDB, l_buf);
     m_inc_counter_fly_file[p_type] = l_sql_command;
    }
    else
        l_sql_command = l_sql_it->second;
    Lock l(m_cs_sqlite);
    sqlite3_transaction l_trans(m_flySQLiteDB);
    l_sql_command->bind(1, (long long int)p_id);
    l_sql_command->executenonquery();
    l_trans.commit();
}
*/
//========================================================================================================
sqlite3_command* CDBManager::bind_sql_counter(const CFlyFileRecordMap& p_sql_array,
                                              bool p_is_get_base_mediainfo)
{
    Lock l(m_cs_sql_cache);
	sqlite3_command* l_sql_command = NULL;
	const size_t l_size_array = p_sql_array.size();
	// Шаг 0. Генерируем SQL запрос для выборки группы значений по ключевой паре TTH + Size.
	CFlyCacheSQLCommandInt& l_pool_sql = p_is_get_base_mediainfo ? m_sql_cache[SQL_CACHE_SELECT_COUNT_MEDIA] : m_sql_cache[SQL_CACHE_SELECT_COUNT];
	CFlyCacheSQLCommandInt::const_iterator l_sql_it = l_pool_sql.find(l_size_array);
	if (l_sql_it == l_pool_sql.end())
	{
		// TODO -
		string l_sql_text =  "select tth,file_size,id,count_query,count_download,count_media,count_antivirus";
		if (p_is_get_base_mediainfo) // Добавим загрузку медиаинфы?
		{
			l_sql_text += ",fly_audio,fly_video,fly_audio_br,fly_xy";
		}
		l_sql_text += " from fly_file where (tth=? and file_size=?)";
		for (int i = 1; i < l_size_array; ++i)
		{
			l_sql_text += " or (tth=? and file_size=?)";
		}
		l_sql_command = new sqlite3_command(m_flySQLiteDB, l_sql_text);
		l_pool_sql.insert(std::make_pair(l_size_array, l_sql_command));
	}
	else
		l_sql_command = l_sql_it->second;
	// Шаг 1. биндим пары переменных TTH+size
	size_t l_bind_index = 1;
	for (CFlyFileRecordMap::const_iterator i = p_sql_array.begin(); i != p_sql_array.end(); ++i)
	{
		l_sql_command->bind(l_bind_index++, i->first.m_tth, SQLITE_TRANSIENT); // SQLITE_STATIC ?
		l_sql_command->bind(l_bind_index++, i->first.m_file_size);
	}
    //static unsigned g_count = 0;
    //static unsigned g_max_size = 0;
    //if (l_size_array > g_max_size)
    //    g_max_size = l_size_array;
    //std::cout << "bind_sql_counter size_array = "<< l_size_array <<" count = " 
    //     << ++g_count << " g_max_size = " << g_max_size << " pthread_self = " << (unsigned long int) pthread_self() << std::endl;
	return l_sql_command;
}
//========================================================================================================
void CDBManager::prepare_insert_fly_file()
{
	m_insert_fly_file.init(m_flySQLiteDB,
			"insert into fly_file (tth,file_size,first_date) values(?,?,strftime('%s','now','localtime'))");
	m_insert_or_replace_fly_file.init(m_flySQLiteDB,
			"insert or replace into fly_file (tth,file_size,first_date) values(?,?,strftime('%s','now','localtime'))"); 
}
//========================================================================================================
long long CDBManager::internal_insert_fly_file(const string& p_tth,sqlite_int64 p_file_size)
{
    static unsigned l_count[2] = {};
    if (p_tth.length() != 39)
    {
        std::cout << "internal_insert_fly_file - p_tth.length() != 39" << std::endl;
    }
	try
	{
        //std::cout << "internal_insert_fly_file - INSERT p_tth=" << p_tth << " count = " << ++l_count[0]  <<std::endl;
        sqlite3_command* l_ins_sql = m_insert_fly_file.get_sql();
		l_ins_sql->bind(1, p_tth, SQLITE_STATIC);
		l_ins_sql->bind(2, p_file_size);
		l_ins_sql->executenonquery();
	}
	catch (const database_error& e)
	{
        std::cout << "internal_insert_fly_file - INSERT or REPLACE p_tth=" << p_tth << " count = " << ++l_count[1] << std::endl;
        sqlite3_command* l_ins_sql = m_insert_or_replace_fly_file.get_sql();
		l_ins_sql->bind(1, p_tth, SQLITE_STATIC);
		l_ins_sql->bind(2, p_file_size);
		l_ins_sql->executenonquery();
	}
	return m_flySQLiteDB.insertid();
}
//========================================================================================================
void CDBManager::internal_process_sql_add_new_fileL()
{
		prepare_insert_fly_file();
}
//========================================================================================================
void CDBManager::process_sql_add_new_fileL(CFlyFileRecordMap& p_sql_array, size_t& p_count_insert)
{
	prepare_insert_fly_file();
	for (CFlyFileRecordMap::iterator i = p_sql_array.begin(); i != p_sql_array.end(); ++i)
	{
		if (i->second.m_fly_file_id == 0)
		{
			i->second.m_fly_file_id = internal_insert_fly_file(i->first.m_tth, i->first.m_file_size);
			i->second.m_count_query = 1;
			i->second.m_count_download = 0;
			i->second.m_count_antivirus = 0;
			i->second.m_is_new_file = true;
			++p_count_insert;
		}
	}
}
//========================================================================================================
void CDBManager::process_sql_counter(CFlyFileRecordMap& p_sql_array,
                                     bool p_is_get_base_mediainfo)
{

	const size_t l_size_array = p_sql_array.size();
	if (l_size_array > 0)
	{
		//Lock l(m_cs_sqlite);
		sqlite3_command* l_sql_command = bind_sql_counter(p_sql_array, p_is_get_base_mediainfo);
		// Шаг 2. Возвращаем результат в виде курсора и сохраняем обратно в мапе
		sqlite3_reader l_q = l_sql_command->executereader();
		while (l_q.read())
		{
			const CFlyFileKey l_tth_key(l_q.getstring(0), l_q.getint64(1));
			CFlyFileRecord& l_rec = p_sql_array[l_tth_key];
			l_rec.m_fly_file_id = l_q.getint64(2);
			l_rec.m_count_query = l_q.getint64(3) + 1; // +1 Т.к. следующей командой пойдет апдейт на инкремент этого параметра
			l_rec.m_count_download = l_q.getint64(4);
			// Кол-во медиаинфы считаем всегда (не сильно накладно) TODO - после анализа версии клиента прикрутить оптимизацию и этой части
			l_rec.m_count_mediainfo = l_q.getint(5);
			l_rec.m_count_antivirus = l_q.getint(6);
			if (p_is_get_base_mediainfo)
			{
				l_rec.m_media.m_audio = l_q.getstring(7);
				if (!l_rec.m_media.m_audio.empty())
					l_rec.m_media.m_audio_br = l_q.getstring(9);
				l_rec.m_media.m_video = l_q.getstring(8);
				if (!l_rec.m_media.m_video.empty())
					l_rec.m_media.m_xy = l_q.getstring(10);
			}
		}
	}
}
//========================================================================================================
void CDBManager::prepare_and_find_all_tth(
    const Json::Value& p_root,
    const Json::Value& p_array,
    CFlyFileRecordMap& p_sql_result_array, // Результат запросов со счетчиком медиаинфы
    CFlyFileRecordMap& p_sql_result_array_only_counter, // Результат запросов только со счетчиком рейтингов
    bool p_is_all_only_counter,
    bool p_is_all_only_ext_info,
    bool p_is_different_ext_info,
    bool p_is_different_counter)
{
	size_t l_count_file_in_json = p_array.size();
	// Обходим входной массив TTH и подготавливаем массив биндинга для последующего исполнения в sql
	for (int j = 0; j < l_count_file_in_json; ++j)
	{
		const Json::Value& l_cur_item_in = p_array[j];
		bool l_is_only_ext_info = p_is_all_only_ext_info;
		if (p_is_different_ext_info == true &&  l_is_only_ext_info == false)
			l_is_only_ext_info = l_cur_item_in["only_ext_info"].asInt() == 1; // Расширенная инфа не нужна для этого элемента?
		bool l_is_only_counter = p_is_all_only_counter;
		if (p_is_different_counter == true && l_is_only_counter == false)
			l_is_only_counter = l_cur_item_in["only_counter"].asInt() == 1; // Только счетчики нужны для этого элемента?
		const CFlyFileKey l_tth_key(l_cur_item_in["tth"].asString(), _atoi64(l_cur_item_in["size"].asString().c_str()));
		if (l_is_only_counter)
		{
			CFlyFileRecord& l_rec = p_sql_result_array_only_counter[l_tth_key];
			l_rec.m_is_only_counter = true;
		}
		else
		{
			CFlyFileRecord& l_rec = p_sql_result_array[l_tth_key];
			l_rec.m_is_only_counter = false;
		}
	}
	process_sql_counter(p_sql_result_array_only_counter, false);
	process_sql_counter(p_sql_result_array, true);
}
//========================================================================================================
int64_t CDBManager::find_tth(const string& p_tth,
                             int64_t p_size,
                             bool p_is_create,
                             Json::Value& p_result_stat,
                             bool p_fill_counter,
                             bool p_calc_count_mediainfo,
                             int64_t& p_count_query,
                             int64_t& p_count_download,
							 int64_t& p_count_antivirus)
{
	string l_marker_crash;
	try
	{
		//Lock l(m_cs_sqlite);
		sqlite3_command* l_sql = 0;
		if (p_calc_count_mediainfo == false)
		{
			l_sql = m_find_tth.init(m_flySQLiteDB,
				                                                           "select id,count_query,count_download,count_antivirus "
#ifdef FLY_SERVER_USE_ALL_COUNTER
				                                                           ",count_plus,count_minus,count_fake,count_upload,first_date"
#endif
				                                                           " from fly_file where file_size=? and tth=?");
		}
		else
		{
			l_sql = m_find_tth_and_count_media.init(m_flySQLiteDB,
				                                                                           "select id,count_query,count_download,count_antivirus,count_media"
#ifdef FLY_SERVER_USE_ALL_COUNTER
				                                                                           ",count_plus,count_minus,count_fake,count_upload,first_date"
#endif
				                                                                           " from fly_file where file_size=? and tth=?");
		}
		l_sql->bind(1, sqlite_int64(p_size));
		l_sql->bind(2, p_tth.c_str(), 39, SQLITE_STATIC);
		int64_t l_id = 0;
#ifdef FLY_SERVER_USE_ALL_COUNTER
		int64_t l_count_plus = 0;
		int64_t l_count_minus = 0;
		int64_t l_count_fake = 0;
		int64_t l_count_upload = 0;
		int64_t l_first_date = 0;
#endif  // FLY_SERVER_USE_ALL_COUNTER
		p_count_query = 0;
		p_count_download = 0;
		p_count_antivirus = 0;
		int l_count_mediainfo = 0;
		// string l_marker_crash = "[+] select from fly_file"; // TODO - Убрать в _DEBUG
		{
			sqlite3_reader l_q = l_sql->executereader();
			if (l_q.read())
			{
				l_id = l_q.getint64(0);
				p_count_query    = l_q.getint64(1);
				p_count_download = l_q.getint64(2);
				p_count_antivirus = l_q.getint64(3);
				if (p_calc_count_mediainfo)
					l_count_mediainfo = l_q.getint(4);
#ifdef FLY_SERVER_USE_ALL_COUNTER
				if (p_calc_count_mediainfo)
				{
					l_count_plus = l_q.getint64(5);
					l_count_minus = l_q.getint64(6);
					l_count_fake = l_q.getint64(7);
					l_count_upload = l_q.getint64(8);
					l_first_date = l_q.getint64(9);
				}
				else
				{
					l_count_plus = l_q.getint64(3);
					l_count_minus = l_q.getint64(4);
					l_count_fake = l_q.getint64(5);
					l_count_upload = l_q.getint64(6);
					l_first_date = l_q.getint64(7);
				}
#endif  // FLY_SERVER_USE_ALL_COUNTER
			}
		}
		const bool l_is_first_tth = !l_id && p_is_create;
		if (l_is_first_tth)
		{
			prepare_insert_fly_file();
			l_id = internal_insert_fly_file(p_tth, p_size);
		}
		++p_count_query;
		if (p_fill_counter)
		{
#ifdef FLY_SERVER_USE_ALL_COUNTER
			if (l_count_plus)
				p_result_stat["count_plus"] = toString(l_count_plus);
			if (l_count_minus)
				p_result_stat["count_minus"] = toString(l_count_minus);
			if (l_count_fake)
				p_result_stat["count_fake"] = toString(l_count_fake);
			if (l_count_upload)
				p_result_stat["count_upload"] = toString(l_count_upload);
#endif // FLY_SERVER_USE_ALL_COUNTER
			if (p_count_download)
				p_result_stat["count_download"] = toString(p_count_download);
			if (p_count_antivirus)
				p_result_stat["count_antivirus"] = toString(p_count_antivirus);
			if (p_count_query)
				p_result_stat["count_query"] = toString(p_count_query);
			// TODO - пока дата регистрации на клиенте не визуализируeтся
			// if(l_first_date)
			// p_result_stat["first_date"] = toString(l_first_date);
		}
		if (l_is_first_tth)
		{
			p_count_query = 0; // Первый раз скидываем счетчик в 0. TODO - отрефакторить
		}
		if (p_calc_count_mediainfo)
		{
			p_result_stat["count_media"] = toString(l_count_mediainfo);
		}
		return l_id;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - find_tth: l_marker_crash = " + l_marker_crash + " error = " + e.getError());
	}
	return 0;
}
//========================================================================================================
#ifdef FLY_SERVER_USE_FLY_DIC
int64_t CDBManager::find_dic_id(const string& p_name, const eTypeDIC p_DIC)
{
	try
	{
		//Lock l_read(m_cs_sqlite);
		if (!m_select_fly_dic.get())
			m_select_fly_dic = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
			                                                                 "select id from fly_dic where name=? and dic=?"));
		sqlite3_command* l_sql = m_select_fly_dic.get();
		l_sql->bind(1, p_name, SQLITE_STATIC);
		l_sql->bind(2, p_DIC);
		int64_t l_dic_id = l_sql->executeint64_no_throw();
		return l_dic_id;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - find_dic_id: " + e.getError());
	}
	return 0;
}
//========================================================================================================
int64_t CDBManager::get_dic_id(const string& p_name, const eTypeDIC p_DIC , bool p_create)
{
	if (p_name.empty())
		return 0;
	try
	{
		int64_t& l_Cache_ID = m_DIC[p_DIC - 1][p_name];
		if (l_Cache_ID)
			return l_Cache_ID;
		l_Cache_ID = find_dic_id(p_name, p_DIC);
		if (!l_Cache_ID)
		{
			if (!m_insert_fly_dic.get())
				m_insert_fly_dic = unique_ptr<sqlite3_command>(new sqlite3_command(m_flySQLiteDB,
				                                                                 "insert into fly_dic (dic,name) values(?,?)"));
			sqlite3_command* l_sql = m_insert_fly_dic.get();
			l_sql->bind(1, p_DIC);
			l_sql->bind(2, p_name, SQLITE_STATIC);
			l_sql->executenonquery();
			l_Cache_ID = m_flySQLiteDB.insertid();
		}
		return l_Cache_ID;
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - getDIC_ID: " + e.getError());
	}
	return 0;
}
#endif
//========================================================================================================
void CDBManager::load_registry(TStringList& p_values, int p_Segment)
{
	p_values.clear();
	CFlyRegistryMap l_values;
	//Lock l_read(m_cs_sqlite);
	load_registry(l_values, p_Segment);
	p_values.reserve(l_values.size());
	for (CFlyRegistryMap::const_iterator k = l_values.begin(); k != l_values.end(); ++k)
		p_values.push_back(k->first);
}
//========================================================================================================
void CDBManager::save_registry(const TStringList& p_values, int p_Segment)
{
	CFlyRegistryMap l_values;
	//Lock l_write(m_cs_sqlite);
	for (TStringList::const_iterator i = p_values.begin(); i != p_values.end(); ++i)
		l_values.insert(CFlyRegistryMap::value_type(
		                    *i,
		                    CFlyRegistryValue()));
	save_registry(l_values, p_Segment);
}
//========================================================================================================
void CDBManager::load_registry(CFlyRegistryMap& p_values, int p_Segment)
{
	//Lock l_read(m_cs_sqlite);
	try
	{
		m_get_registry.init(m_flySQLiteDB,
			                                                               "select key,val_str,val_number from fly_registry where segment=?");
		m_get_registry->bind(1, p_Segment);
		sqlite3_reader l_q = m_get_registry->executereader();
		while (l_q.read())
			p_values.insert(CFlyRegistryMap::value_type(
			                    l_q.getstring(0),
			                    CFlyRegistryValue(l_q.getstring(1), l_q.getint64(2))));
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - load_registry: " + e.getError());
	}
}
//========================================================================================================
void CDBManager::save_registry(const CFlyRegistryMap& p_values, int p_Segment)
{
	const sqlite_int64 l_tick = get_tick_count();
	//Lock l_write(m_cs_sqlite);
	try
	{
		sqlite3_transaction l_trans(m_flySQLiteDB);
		m_insert_registry.init(m_flySQLiteDB,
			                                                                  "insert or replace into fly_registry (segment,key,val_str,val_number,tick_count) values(?,?,?,?,?)");
		sqlite3_command* l_sql = m_insert_registry.get_sql();
		for (CFlyRegistryMap::const_iterator k = p_values.begin(); k != p_values.end(); ++k)
		{
			l_sql->bind(1, p_Segment);
			l_sql->bind(2, k->first, SQLITE_TRANSIENT);
			l_sql->bind(3, k->second.m_val_str, SQLITE_TRANSIENT);
			l_sql->bind(4, k->second.m_val_int64);
			l_sql->bind(5, l_tick);
			l_sql->executenonquery();
		}
		m_delete_registry.init(m_flySQLiteDB,
			                                                                  "delete from fly_registry where segment=? and tick_count<>?");
		m_delete_registry->bind(1, p_Segment);
		m_delete_registry->bind(2, l_tick);
		m_delete_registry->executenonquery();
		l_trans.commit();
	}
	catch (const database_error& e)
	{
		errorDB("SQLite - save_registry: " + e.getError());
	}
}
#endif // FLY_SERVER_USE_SQLITE
//========================================================================================================
void CDBManager::shutdown()
{
#ifdef FLY_SERVER_USE_SQLITE
	g_exclude_fly_dic_transform.clear();
#endif
}
//========================================================================================================
CDBManager::~CDBManager()
{
	std::cout << std::endl << "* fly-server CDBManager::~CDBManager" << std::endl;
#ifdef FLY_SERVER_USE_SQLITE

	flush_inc_counter_fly_file();
    Lock l(m_cs_sql_cache);
	for (int j = 0; j < SQL_CACHE_LAST; ++j)
	{
		for (CFlyCacheSQLCommandInt::iterator i = m_sql_cache[j].begin(); i != m_sql_cache[j].end(); ++i)
		{
			delete i->second;
		}
	}
#endif // FLY_SERVER_USE_SQLITE	
#ifndef _WIN32
	syslog(LOG_NOTICE, "CDBManager destroy!");
	closelog();
#endif
	std::cout << std::endl << "* fly-server destroy!" << std::endl;
}
//========================================================================================================
