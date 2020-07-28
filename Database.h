#ifndef Database_h
#define Database_h

#include <tuple>
#include <deque>
#include <mutex>
#include <string>
#include <mutex>
#include <condition_variable>

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "Database.h"

/// \TODO: clean these globabls up and move look_for_changes() to here.

class DbStatus;

//Some global variables to control the thread updating data from the database
extern bool g_keep_watching_db;
extern std::mutex g_watch_db_mutex;
extern std::condition_variable g_update_cv;



//We will keep some data from the DB in memmory for when new sessions are
//  started - memorry isnt an issue with this data, and its just easier for now
extern std::mutex g_data_mutex;
extern std::deque<std::tuple<Wt::WDateTime,int,bool>> g_oxygen_values;
extern std::deque<std::tuple<Wt::WDateTime,int,bool>> g_heartrate_values;  //<time as string,value,moving>
extern std::deque<DbStatus> g_statuses;

//  Each entry probably takes less than 50 bytes, so 200k entries is like 10 MB.
//  We are getting maybe like 1 reading per 30 seconds, so 200k is like 2 months of data.
const size_t g_max_data_entries = 200000;

void look_for_db_changes();

/** Converts from the string formats we have in the UTC fields in the database, to a WDateTime.
 Not extensively tested, and I still havent investigated why there are a few different date/time formats in the database.
 */
Wt::WDateTime utcDateTimeFromStr( const std::string &dt );


//A class to store heartrate or oxygenation levels in
class DbValue
{
public:
  //The database -> WDateTime doesnt seem to work correctly, so we will read date/time as string
  //  from database, and then use #utcDateTimeFromStr to convert to a WDateTime.
  //Wt::WDateTime local_date;
  //Wt::WDateTime utc_date;
  std::string local_date;
  std::string utc_date;
  
  double value;
  int movement;
  int sock_off;
  int base_station;
  
  template<class Action>
  void persist(Action& a)
  {
    Wt::Dbo::field(a, local_date,   "local_date");
    Wt::Dbo::field(a, utc_date,     "utc_date");
    Wt::Dbo::field(a, value,        "value");
    Wt::Dbo::field(a, movement,     "movement");
    Wt::Dbo::field(a, sock_off,     "sock_off");
    Wt::Dbo::field(a, base_station, "base_station");
  }
};//class DbValue



class DbStatus
{
public:
  //Wt::WDateTime local_date;
  //Wt::WDateTime utc_date;
  std::string local_date;
  std::string utc_date;
  
  int movement;
  int sock_off;
  int base_station;
  int sock_connection;
  int battery;
  
  template<class Action>
  void persist(Action& a)
  {
    Wt::Dbo::field(a, local_date,   "local_date");
    Wt::Dbo::field(a, utc_date,     "utc_date");
    Wt::Dbo::field(a, movement,     "movement");
    Wt::Dbo::field(a, sock_off,     "sock_off");
    Wt::Dbo::field(a, base_station, "base_station");
    Wt::Dbo::field(a, sock_connection, "sock_connection");
    Wt::Dbo::field(a, battery,      "battery");
  }
};//class DbStatus


//Even though DbOxygen and DbHeartrate are identical, we need
//  seperate classes or else Dbo wont make seperate tables.
class DbOxygen : public DbValue
{
};

class DbHeartrate : public DbValue
{
};




#endif //Database_h
