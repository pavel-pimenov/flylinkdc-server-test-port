//-----------------------------------------------------------------------------
//(c) 2007-2021 pavel.pimenov@gmail.com
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

#include "civetweb/civetweb.h"

//#define FLY_SERVER_USE_ONLY_TEST_PORT
#ifndef FLY_SERVER_USE_ONLY_TEST_PORT
#define FLY_SERVER_USE_SQLITE
#endif

#ifdef FLY_SERVER_USE_SQLITE
#include "sqlite/sqlite3x.hpp"
using sqlite3x::sqlite3_command;
using sqlite3x::sqlite3_connection;
#else
typedef long long int sqlite_int64;
#endif

//============================================================================================
extern bool g_setup_log_disable_set;
extern bool g_setup_log_disable_stat;
extern bool g_setup_log_disable_download;
extern bool g_setup_log_disable_download_torrent;
extern bool g_setup_log_disable_antivirus;
extern bool g_setup_log_disable_login;
extern bool g_setup_log_disable_test_port;
extern bool g_setup_disable_ip_stat;
extern bool g_setup_log_disable_error_sql;
extern bool g_setup_syslog_disable;

//============================================================================================
bool zlib_uncompress(const uint8_t* p_zlib_source, size_t p_zlib_len, std::vector<unsigned char>& p_decompress);
bool zlib_compress(const char* p_source, size_t p_len, std::vector<unsigned char>& p_compress, int& p_zlib_result, int p_level = 9);
//============================================================================================

// #define FLY_SERVER_USE_STORE_LOST_LOCATION

sqlite_int64 get_tick_count();

namespace MediaInfoLib
{
enum stream_t
{
	Stream_General,                 ///< StreamKind = General
	Stream_Video,                   ///< StreamKind = Video
	Stream_Audio,                   ///< StreamKind = Audio
	Stream_Text,                    ///< StreamKind = Text
	Stream_Chapters,                ///< StreamKind = Chapters
	Stream_Image,                   ///< StreamKind = Image
	Stream_Menu,                    ///< StreamKind = Menu
	Stream_Max
};
}

enum eTypeQuery
{
	FLY_POST_QUERY_UNKNOWN = 0,
//#ifdef FLY_SERVER_USE_SQLITE
	FLY_POST_QUERY_GET = 1,
	FLY_POST_QUERY_SET = 2,
	FLY_POST_QUERY_LOGIN = 3, // TODO - удалить
//#endif
	FLY_POST_QUERY_STATISTIC = 4,
	FLY_POST_QUERY_ERROR_SQL = 5,
	FLY_POST_QUERY_TEST_PORT = 6,
//#ifdef FLY_SERVER_USE_SQLITE
	FLY_POST_QUERY_DOWNLOAD = 7,
	FLY_POST_QUERY_ANTIVIRUS = 8,
	FLY_POST_QUERY_DOWNLOAD_TORRENT = 9
	//#endif
};


#ifdef _WIN32
//#define snprintf _snprintf
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
		unsigned m_count_file_in_json;
		size_t m_count_cache;
		size_t m_count_get_only_counter;
		size_t m_count_get_base_media_counter;
		size_t m_count_get_ext_media_counter;
		size_t m_count_insert;
		unsigned m_content_len;
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
			return 
#ifdef FLY_SERVER_USE_SQLITE
				m_query_type == FLY_POST_QUERY_GET ||
			       m_query_type == FLY_POST_QUERY_SET ||
			       m_query_type == FLY_POST_QUERY_LOGIN ||
			       m_query_type == FLY_POST_QUERY_DOWNLOAD ||
				m_query_type == FLY_POST_QUERY_DOWNLOAD_TORRENT ||
				m_query_type == FLY_POST_QUERY_ANTIVIRUS ||
#endif
				m_query_type == FLY_POST_QUERY_STATISTIC ||
				m_query_type == FLY_POST_QUERY_ERROR_SQL ||
				m_query_type == FLY_POST_QUERY_TEST_PORT;
		}
#ifdef FLY_SERVER_USE_SQLITE
		bool is_get_or_set_query() const
		{
			return m_query_type == FLY_POST_QUERY_GET || m_query_type == FLY_POST_QUERY_SET;
		}
#endif
		unsigned long long get_delta_db() const
		{
			return m_tick_count_stop_db - m_tick_count_start_db;
		}
		unsigned get_http_len() const
		{
			if (m_is_zlib)
				return unsigned(m_dest_data.size());
			else
				return unsigned(m_res_stat.length());
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
			if (m_decompress.empty()) // TODO проверять по коду возврата декомпрессии - чтобы 0x78 не слать по ошибке?
				m_in_query = string(p_content, p_content_len);
			else
				m_in_query = string(reinterpret_cast<char*>(m_decompress.data()), m_decompress.size());
		}
		unsigned get_real_query_size() const
		{
			if (m_decompress.empty())
				return unsigned(m_content_len);
			else
				return unsigned(m_decompress.size());
		}
		void comress_result()
		{
			m_is_zlib = false;
			if (!m_res_stat.empty() && m_is_zlib_result) // Клиент просит сжать ответ zlib?
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
#ifdef FLY_SERVER_USE_SQLITE
				if (!strncmp(p_uri, "/fly-zget", 9))
				{
					// - fly-zget
					// - fly-zget-full
					m_query_type = FLY_POST_QUERY_GET;
				}
				else if (!strcmp(p_uri, "/fly-set"))
				{
					m_query_type = FLY_POST_QUERY_SET;
				}
				else if (!strcmp(p_uri, "/fly-download"))
				{
					m_query_type = FLY_POST_QUERY_DOWNLOAD;
				}
				else if (!strcmp(p_uri, "/fly-download-torrent"))
				{
					m_query_type = FLY_POST_QUERY_DOWNLOAD_TORRENT;
				}
				else if (!strcmp(p_uri, "/fly-antivirus"))
				{
					m_query_type = FLY_POST_QUERY_ANTIVIRUS;
				}
				else if (!strcmp(p_uri, "/fly-get")) // Старые клиенты продолжают слать fly-get но скоро отомрут + саоме важное они не умеют расжимать контент
				{
					m_is_zlib_result = false;
					m_query_type = FLY_POST_QUERY_GET;
				}
				else if (!strcmp(p_uri, "/fly-login"))
				{
					m_query_type = FLY_POST_QUERY_LOGIN;
				}
				else
#endif // FLY_SERVER_USE_SQLITE
				if (!strcmp(p_uri, "/fly-stat"))
				{
					m_query_type = FLY_POST_QUERY_STATISTIC;
				}
				else if (!strcmp(p_uri, "/fly-error-sql"))
				{
					m_query_type = FLY_POST_QUERY_ERROR_SQL;
				}
				else if (!strcmp(p_uri, "/fly-test-port"))
				{
					m_is_zlib_result = false; // Ответ не жмем
					m_query_type = FLY_POST_QUERY_TEST_PORT;
				}

			}
		}
};
//================================================================================
struct CFlyFileKey
{
	std::string   m_tth;
	sqlite_int64  m_file_size;
	CFlyFileKey(const std::string& p_tth, sqlite_int64 p_file_size): m_file_size(p_file_size), m_tth(p_tth)
	{
	}
	CFlyFileKey(): m_file_size(0)
	{
	}
	bool operator < (const CFlyFileKey& p_val) const
	{
		return m_file_size < p_val.m_file_size || (m_file_size == p_val.m_file_size && m_tth < p_val.m_tth);
	}
};
//================================================================================
struct CFlyBaseMediainfo
{
	std::string m_video;
	std::string m_audio;
	std::string m_audio_br;
	std::string m_xy;
};
//================================================================================
struct CFlyFileRecord
{
	sqlite_int64  m_fly_file_id;
	sqlite_int64  m_count_query;
	sqlite_int64  m_count_download;
	sqlite_int64  m_count_antivirus;
	uint16_t      m_count_mediainfo; // TODO - материализовать?
	bool          m_is_only_counter; // TODO - пока не используется. не придумал
	bool          m_is_new_file;
//	bool          m_is_calc_count_mediainfo;

	CFlyBaseMediainfo m_media;
	CFlyFileRecord() :
		m_fly_file_id(0),
		m_count_query(0),
		m_count_download(0),
		m_count_antivirus(0),
		m_count_mediainfo(0),
		m_is_only_counter(false),
		m_is_new_file(false)
//		m_is_calc_count_mediainfo(false),
	{
	}
};
//================================================================================
class CFlyFileRecordMap : public std::map<CFlyFileKey , CFlyFileRecord>
{
	public:
		bool is_new_file() const
		{
			for (const_iterator i = begin(); i != end(); ++i)
				if (i->second.m_fly_file_id == 0)
					return true;
			return false;
		}
};
//================================================================================
struct CFlyThreadBase
{
	CDBManager*  m_db;
	CFlyThreadBase(CDBManager* p_db):
		m_db(p_db)
	{
	}
};
//================================================================================
struct CFlyThreadUpdaterInfo : public CFlyThreadBase
{
	CFlyFileRecordMap* m_file_full;
	CFlyFileRecordMap* m_file_only_counter;
	CFlyThreadUpdaterInfo(CDBManager* p_db): CFlyThreadBase(p_db),
		m_file_full(NULL),
		m_file_only_counter(NULL)
	{
	}
	~CFlyThreadUpdaterInfo()
	{
		delete m_file_full;
		delete m_file_only_counter;
	}
};
//================================================================================
#ifdef FLY_SERVER_USE_SQLITE
class CFlySQLCommand
{
public:
	CFlySQLCommand() {}
	sqlite3_command* get_sql()
	{
		return m_sql.get();
	}
	sqlite3_command* operator->()
	{
		return m_sql.get();
	}
	sqlite3_command* init(sqlite3_connection& p_db, const char* p_sql)
	{
		if (!m_sql.get())
		{
			m_sql = unique_ptr<sqlite3_command>(new sqlite3_command(p_db, p_sql));
		}
		return m_sql.get();
	}
	int sqlite3_changes() const
	{
		return m_sql->get_connection().sqlite3_changes();
	}
private:
	unique_ptr<sqlite3_command> m_sql;
};
#endif // FLY_SERVER_USE_SQLITE
//================================================================================
class CDBManager
{
	public:
		CDBManager() {};
		~CDBManager();
	
		void init();

		void shutdown();
#ifdef FLY_SERVER_USE_SQLITE
		std::string store_media_info(CFlyServerContext& p_flyserver_cntx);


		void load_registry(CFlyRegistryMap& p_values, int p_Segment);
		void save_registry(const CFlyRegistryMap& p_values, int p_Segment);
		void load_registry(TStringList& p_values, int p_Segment);
		void save_registry(const TStringList& p_values, int p_Segment);

		void zlib_convert_attr_value();
		string login(const string& p_JSON, const string& p_IP);
		sqlite3_connection m_flySQLiteDB;
		void add_download_tth(const string& p_json);
		void add_download_torrent(const string& p_json);
		void antivirus_inc(const string& p_json);
#endif
private:
#ifdef FLY_SERVER_USE_SQLITE
		bool is_table_exists(const string& p_table_name);
		void pragma_executor(const char* p_pragma);
		
		CFlySQLCommand m_find_tth;
		CFlySQLCommand m_find_tth_and_count_media;
#ifdef FLY_SERVER_USE_FLY_DIC
		CFlySQLCommand m_select_fly_dic;
		CFlySQLCommand m_insert_fly_dic;
#endif
		CFlySQLCommand m_insert_fly_file;
		CFlySQLCommand m_insert_or_replace_fly_file;
		void prepare_insert_fly_file();
		long long internal_insert_fly_file(const string& p_tth, sqlite_int64 p_file_size);

		std::unordered_map<sqlite_int64, unsigned> m_count_query_cache;
		CriticalSection m_cs_inc_counter_file;
		
		std::map<CFlyFileKey, unsigned> m_download_file_stat_array; // TODO unordered_map
		CriticalSection m_cs_inc_download_file;

		std::map<CFlyFileKey, unsigned> m_antivirus_file_stat_array; // TODO unordered_map
		CriticalSection m_cs_inc_antivirus_file;

        std::string get_ext_gzip_json(const std::string&  p_ttp);
        CFlySQLCommand m_insert_tth_json;
        CFlySQLCommand m_select_tth_json;
        
        CFlySQLCommand m_insert_fly_file_download;
		CFlySQLCommand m_insert_fly_file_antivirus;
		CFlySQLCommand m_update_antivirus_count;

#ifndef FLY_SERVER_USE_ARRAY_UPDATE
		CFlySQLCommand m_update_inc_count_query;
#endif

#ifdef FLY_SERVER_USE_STORE_LOST_LOCATION
		unique_ptr<sqlite3_command> m_insert_fly_location_ip_lost;
#endif
		CFlySQLCommand m_select_fly_file;
		CFlySQLCommand m_insert_attr_array;
		CFlySQLCommand m_select_attr_array;
		
		CFlySQLCommand m_insert_base_attr;
		
		CFlySQLCommand m_select_attr_array_only_ext;
		
		CFlySQLCommand m_get_registry;
		CFlySQLCommand m_insert_registry;
		CFlySQLCommand m_delete_registry;
		
		CFlySQLCommand m_zlib_convert_select;
		CFlySQLCommand m_zlib_convert_update;

		CFlySQLCommand m_update_download_count;
		
		typedef std::map<int, sqlite3_command* > CFlyCacheSQLCommandInt;
		enum
		{
			SQL_CACHE_SELECT_COUNT          = 0,
			SQL_CACHE_SELECT_COUNT_MEDIA    = 1,
#ifdef FLY_SERVER_USE_ARRAY_UPDATE
			SQL_CACHE_UPDATE_COUNT          = 2,
#endif
			SQL_CACHE_LAST
		};
        CriticalSection m_cs_sql_cache;
		CFlyCacheSQLCommandInt m_sql_cache[SQL_CACHE_LAST]; // Кэш для паетного запроса получения
		// 0. счетчика count_query,
		// 1. счетчика count_query + count(*) из медиаинфы
		// 2. Кэш для массового апдейта счетчика count_query
		
	public:
		void flush_inc_counter_fly_file();
	private:
	
	
		int64_t find_tth(const string& p_tth,
		                 int64_t p_size,
		                 bool p_create,
		                 Json::Value& p_result_stat,
		                 bool p_fill_counter,
		                 bool p_calc_count_mediainfo,
		                 int64_t& p_count_query, // Возвращаем счетчик. если >1 то выполняем массовый апдейт +1
		                 int64_t& p_count_download,
						 int64_t& p_count_antivirus
		                );
		void process_store_json_ext_info(const string& p_tth,
		                                 const int64_t p_id_file,
		                                 const Json::Value& p_cur_item_in);
		void process_get_query(const size_t p_index_result,
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
		                       size_t& p_only_ext_info_counter
		                      );
		void process_sql_counter(CFlyFileRecordMap& p_sql_array,
		                         bool p_is_get_base_mediainfo);
	public:
		void process_sql_add_new_fileL(CFlyFileRecordMap& p_sql_array,
		                               size_t& p_count_insert);
		void internal_process_sql_add_new_fileL();
	private:
		sqlite3_command* bind_sql_counter(const CFlyFileRecordMap& p_sql_array,
		                                  bool p_is_get_base_mediainfo);
		void prepare_and_find_all_tth(const Json::Value& p_root,
		                              const Json::Value& p_array,
		                              CFlyFileRecordMap& p_sql_result_array,
		                              CFlyFileRecordMap& p_sql_result_array_only_counter,
		                              bool p_is_all_only_counter,
		                              bool p_is_all_only_ext_info,
		                              bool p_is_different_ext_info,
		                              bool p_is_different_counter);
#ifdef FLY_SERVER_USE_FLY_DIC
		int64_t get_dic_id(const string& p_name, const eTypeDIC p_DIC, bool p_create);
		int64_t find_dic_id(const string& p_name, const eTypeDIC p_DIC);
		typedef std::map<std::string, int64_t> CFlyCacheDIC;
		std::vector<CFlyCacheDIC> m_DIC;
#endif		
		// CriticalSection gm_cs_Writer;
		// CriticalSection gm_cs_Reader;
	public: // TOOD - fix public
		CriticalSection m_cs_sqlite;
	private:
		std::set<std::string> g_exclude_fly_dic_transform; // Коллекция тэгов предназначенных для помещения в базу в виде сырых значения (без проброса в справочники)
		
		class CFlyMediaAttrItem
		{
			public:
				std::string m_param;
				std::string m_value;
				int64_t m_dic_attr_id;
				int64_t m_dic_value_id;
				uint8_t m_stream_type; // MediaInfoLib::stream_t
				uint8_t m_channel;
				CFlyMediaAttrItem(const std::string& p_param, const int p_value, uint8_t p_stream_type = 0, uint8_t p_channel = 0)
					: m_stream_type(p_stream_type), m_channel(p_channel),
					  m_param(p_param), m_value(toString(p_value)), m_dic_value_id(0), m_dic_attr_id(0)
				{
				}
				CFlyMediaAttrItem(const std::string& p_param, const std::string& p_value, uint8_t p_stream_type = 0, uint8_t p_channel = 0)
					: m_stream_type(p_stream_type), m_channel(p_channel),
					  m_param(p_param), m_value(p_value), m_dic_value_id(0), m_dic_attr_id(0)
				{
				}
		};
		typedef std::vector<CFlyMediaAttrItem> CFlyMediaAttrArray;
		
		// void set_attr_array(sqlite_int64 p_file_id, CFlyMediaAttrArray& p_array);
		void set_base_media_attr(int64_t p_file_id, const CFlyBaseMediainfo& p_media, int p_count_media);
#ifdef FLY_SERVER_USE_FLY_DIC
		int64_t calc_dic_value_id(const string& p_param, const string& p_value, int64_t& p_dic_attr_id, bool p_read_only);
#endif
		
		static void errorDB(const string& p_txt, bool p_is_exit_process = true);
		bool safeAlter(const char* p_sql);
		static string encodeURI(const string& aString, bool reverse = false);
#endif // FLY_SERVER_USE_SQLITE		
		
};

#endif
