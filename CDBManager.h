//-----------------------------------------------------------------------------
//(c) 2007-2019 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#ifndef CDBManager_H
#define CDBManager_H

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <set>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>
#include <stdint.h>

#ifdef _WIN32
#include <process.h>
#include "zlib/zlib.h"
#else
#include <sys/time.h>
#include <zlib.h>
#include <errno.h>
#endif

#include "Thread.h"

#include "json/json.h"

//#define FLY_SERVER_USE_ONLY_TEST_PORT
#ifndef FLY_SERVER_USE_ONLY_TEST_PORT
#define FLY_SERVER_USE_SQLITE
#endif

typedef long long int sqlite_int64;

//============================================================================================
extern bool g_setup_log_disable_test_port;
extern bool g_setup_syslog_disable;
//============================================================================================
bool zlib_uncompress(const uint8_t* p_zlib_source, size_t p_zlib_len, std::vector<unsigned char>& p_decompress);
bool zlib_compress(const char* p_source, size_t p_len, std::vector<unsigned char>& p_compress, int& p_zlib_result, int p_level = 9);
//============================================================================================


sqlite_int64 get_tick_count();

enum eTypeQuery
{
	FLY_POST_QUERY_UNKNOWN = 0,
	FLY_POST_QUERY_TEST_PORT = 6,
};


#ifdef _WIN32
#define snprintf _snprintf
#else
#define _atoi64 atoll
#endif

using std::unique_ptr;
//==========================================================================
inline std::string toString(long long p_val)
{
	char l_buf[24];
	l_buf[0] = 0;
	snprintf(l_buf, sizeof(l_buf), "%lld", p_val);
	return l_buf;
}
//================================================================================
#ifdef FLY_SERVER_USE_FLY_DIC
enum eTypeDIC
{
	e_DIC_MEDIA_ATTR_TYPE = 1,
	e_DIC_MEDIA_ATTR_VALUE = 2,
	e_DIC_LAST
};
#endif
#ifdef FLY_SERVER_USE_SQLITE
//==========================================================================
enum eTypeRegistrySegment
{
	e_Statistic = 1
};
//==========================================================================
struct CFlyRegistryValue
{
	sqlite_int64  m_val_int64;
	std::string m_val_str;
	CFlyRegistryValue(unsigned long long p_val_int64 = 0) :
		m_val_int64(p_val_int64)
	{
	}
	CFlyRegistryValue(const std::string &p_str, sqlite_int64 p_val_int64 = 0) :
		m_val_int64(p_val_int64),
		m_val_str(p_str)
	{
	}
	operator sqlite_int64() const
	{
		return m_val_int64;
	}
};
//==========================================================================
typedef std::map<string, CFlyRegistryValue> CFlyRegistryMap;
#endif // FLY_SERVER_USE_SQLITE
//==========================================================================
struct CFlyPortTestThreadInfo
{
	std::string m_ip;
	std::string m_CID;
	std::string m_PID;
	std::vector<std::pair<std::string, bool> > m_ports; // second == true - is_tcp
  const char* get_type_port(int p_index) const
  {
      return m_ports[p_index].second ? "TCP" : "UDP";
  }
};
//==========================================================================
class CFlyLogThreadInfo
{
	public:
		eTypeQuery m_query_type;
		std::string m_remote_ip;
		std::string m_in_query;
		time_t m_now;
		CFlyLogThreadInfo(eTypeQuery p_query_type, const std::string& p_remote_ip, const std::string& p_in_query):
			m_query_type(p_query_type),
			m_remote_ip(p_remote_ip),
			m_in_query(p_in_query)
		{
			time(&m_now);
		}
private:
		CFlyLogThreadInfo()
		{
		}
};
typedef std::vector<CFlyLogThreadInfo> CFlyLogThreadInfoArray;
//==========================================================================
class CDBManager;
class CFlyServerContext
{
	public:
		eTypeQuery m_query_type;
		bool m_is_zlib;
		bool m_is_zlib_result;
		std::string m_in_query;
		std::string m_res_stat;
		std::string m_fly_response;
		std::string m_user_agent;
		std::string m_uri;
		std::string m_remote_ip;
		std::string m_error;
		std::vector<unsigned char> m_dest_data;
		std::vector<unsigned char> m_decompress;
		int64_t m_tick_count_start_db;
		int64_t m_tick_count_stop_db;
		size_t m_count_file_in_json;
		size_t m_count_cache;
		size_t m_count_get_only_counter;
		size_t m_count_get_base_media_counter;
		size_t m_count_get_ext_media_counter;
		size_t m_count_insert;
		size_t m_content_len;
		static CFlyLogThreadInfoArray* g_log_array;
		CFlyServerContext():
			m_query_type(FLY_POST_QUERY_UNKNOWN),
			m_is_zlib(false),
			m_is_zlib_result(true),
			m_tick_count_start_db(0),
			m_tick_count_stop_db(0),
			m_count_file_in_json(0),
			m_count_cache(0),
			m_count_get_only_counter(0),
			m_count_get_base_media_counter(0),
			m_count_get_ext_media_counter(0),
			m_count_insert(0),
			m_content_len(0)
		{
		}
		void send_syslog() const;
		void run_thread_log();
		static void flush_log_array(bool p_is_force);
		void run_db_query(const char* p_content, size_t p_len, CDBManager& p_DB);
		
		bool is_valid_query() const
		{
			return m_query_type == FLY_POST_QUERY_TEST_PORT;
		}
		unsigned long long get_delta_db() const
		{
			return m_tick_count_stop_db - m_tick_count_start_db;
		}
		size_t get_http_len() const
		{
			if (m_is_zlib)
				return m_dest_data.size();
			else
				return m_res_stat.length();
		}
		const char* get_result_content()
		{
			if (m_is_zlib)
				return reinterpret_cast<const char*>(m_dest_data.data());
			else
				return m_res_stat.c_str();
		}
		char get_compress_flag() const
		{
			return m_decompress.empty() ? ' ' : 'Z';
		}
		void init_uri(const char* p_uri, const char* p_user_agent, const char* p_fly_response)
		{
			if (p_uri)
			{
				m_uri = p_uri;
				calc_query_type(p_uri);
				if (p_user_agent)
				{
					m_user_agent = p_user_agent;
				}
				if (p_fly_response)
				{
					m_fly_response = p_fly_response;
				}
			}
		}
		void init_in_query(const char* p_content, size_t p_content_len)
		{
			if (m_decompress.empty()) // TODO ��������� �� ���� �������� ������������ - ����� 0x78 �� ����� �� ������?
				m_in_query = string(p_content, p_content_len);
			else
				m_in_query = string(reinterpret_cast<char*>(m_decompress.data()), m_decompress.size());
		}
		size_t get_real_query_size() const
		{
			if (m_decompress.empty())
				return m_content_len;
			else
				return m_decompress.size();
		}
		void comress_result()
		{
			m_is_zlib = false;
			if (!m_res_stat.empty() && m_is_zlib_result) // ������ ������ ����� ����� zlib?
			{
				int l_zlib_result;
				m_is_zlib = zlib_compress(m_res_stat.c_str(), m_res_stat.size(), m_dest_data, l_zlib_result, 6);
				if (!m_is_zlib)
				{
					std::cout << "compression failed l_zlib_result=" <<   l_zlib_result <<
					          " l_dest_data.size() = " <<  m_dest_data.size() <<
					          " l_flyserver_cntx.m_res_stat.length() = " << m_res_stat.length() << std::endl;
#ifndef _WIN32
					syslog(LOG_NOTICE, "compression failed l_zlib_result = %d l_dest_length = %u m_res_stat.length() = %u",
					       l_zlib_result, unsigned(m_dest_data.size()), unsigned(m_res_stat.length()));
#endif
				}
			}
		}
		
		static std::string get_json_file_name(const char* p_name_dir, const char* p_ip, time_t p_now)
		{
			char l_time_buf[32];
			l_time_buf[0] = 0;
			strftime(l_time_buf, sizeof(l_time_buf), "%Y-%m-%d-%H-%M-%S", gmtime(&p_now));
			static int l_count_uid = 0;
			char l_result_buf[256];
			l_result_buf[0] = 0;
			snprintf(l_result_buf, sizeof(l_result_buf), "%s/%s-%s-%d-pid-%d-%lu.json",
			         p_name_dir, p_ip, l_time_buf, ++l_count_uid, getpid(), pthread_self());
			return l_result_buf;
		}
		
		void log_error()
		{
			if (!m_error.empty())
			{
				time_t l_now;
				time(&l_now);
				const string l_file_name = get_json_file_name("log-internal-sqlite-error", m_remote_ip.c_str(), l_now);
				std::fstream l_log_json(l_file_name.c_str(), std::ios_base::out | std::ios_base::trunc);
				if (!l_log_json.is_open())
				{
					std::cout << "Error open file: " << l_file_name;
#ifndef _WIN32
					syslog(LOG_NOTICE, "Error open file: = %s", l_file_name.c_str());
#endif
				}
				else
				{
					l_log_json.write(m_in_query.c_str(), m_in_query.length());
					l_log_json << std::endl << "Error:" << std::endl << m_error;
				}
			}
		}
	private:
		void calc_query_type(const char* p_uri)
		{
			m_query_type = FLY_POST_QUERY_UNKNOWN;
			m_is_zlib_result = true;
			if (p_uri)
			{
				if (!strcmp(p_uri, "/fly-test-port"))
				{
					m_is_zlib_result = false; // ����� �� ����
					m_query_type = FLY_POST_QUERY_TEST_PORT;
				}

			}
		}
};
//================================================================================
class CDBManager
{
	public:
		CDBManager() {};
		~CDBManager();
		void init();

		void shutdown();
private:
};

#endif
