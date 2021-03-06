#include <iostream>
//#include <filesystem>

//looks like the current raspberrypi default compiler doesnt support filesystem::last_write_time()
#include <boost/filesystem.hpp>

#include <Wt/Dbo/backend/Sqlite3.h>

#include "Database.h"
#include "OwletWebApp.h"

using namespace std;
using namespace Wt;

namespace dbo = Wt::Dbo;


Wt::WDateTime utcDateTimeFromStr( const string &str )
{
  Wt::WDateTime utc = WDateTime::fromString(str, "yyyy-MM-ddThh:mm:ssZ");
  if( !utc.isValid() )
    utc = WDateTime::fromString(str, "yyyy-MM-dd hh:mm:ss");
  if( !utc.isValid() )
  {
    const auto pos = str.find(".");  //
    if( pos != string::npos )
      utc = WDateTime::fromString(str.substr(0,pos), "yyyy-MM-dd hh:mm:ss");
  }
  
  //cout << str << " --> utc.date().day()=" << utc.date().day() << ", time=" << utc.time().hour()
  //     << ":" << utc.time().minute() << ":" << utc.time().second() << endl;
  
  return utc;
}//Wt::WDateTime utcDateTimeFromStr( const string &str )

bool g_keep_watching_db = true;
std::mutex g_watch_db_mutex;
std::condition_variable g_update_cv;



//We will keep some data from the DB in memmory for when new sessions are
//  started - memorry isnt an issue with this data, and its just easier for now
std::mutex g_data_mutex;
std::deque<std::tuple<Wt::WDateTime,int,bool>> g_oxygen_values;
std::deque<std::tuple<Wt::WDateTime,int,bool>> g_heartrate_values;  //<time as string,value,moving>
std::deque<OwletReading> g_readings;
std::deque<DbStatus> g_statuses;


//Customize the schema Wt uses to match what is defined in the Python script
namespace Wt
{
  namespace Dbo
  {
    template<>
    struct dbo_traits<DbOxygen> : public dbo_default_traits {
      static const char *versionField() { return 0; }
    };
    
    template<>
    struct dbo_traits<DbHeartrate> : public dbo_default_traits
    {
      static const char *versionField() { return 0; }
    };
  
    template<>
    struct dbo_traits<DbStatus> : public dbo_default_traits
    {
      static const char *versionField() { return 0; }
    };
  }//namespace Dbo
}//namespace Wt


void look_for_db_changes()
{
  const string databasefile = "data/owlet_data.db";
  dbo::Session session;
  
  {
    auto sqlite3 = make_unique<dbo::backend::Sqlite3>(databasefile);
    //sqlite3->setProperty("show-queries", "true");
    session.setConnection( std::move(sqlite3) );
  }

  session.mapClass<DbOxygen>("Oxygen");
  session.mapClass<DbHeartrate>("HeartRate");
  session.mapClass<DbStatus>("Status");
  
  
  try
  {
    session.createTables();
    cout << "Created Tables" << endl;
  }catch( dbo::Exception &e )
  {
    //cout << "While createing table: " << e.what() << endl;
  }
  
  size_t iteration_num = 0;
  int last_oxygen_index = -1, last_heartrate_index = -1, last_status_index = -1;
  //std::filesystem::file_time_type last_write = std::filesystem::file_time_type::min();
  std::time_t last_write = std::numeric_limits<std::time_t>::min();
  
  WDateTime prev_o2, prev_heart;
  
  while( true )
  {
    try
    {
      std::time_t this_write = boost::filesystem::last_write_time( databasefile );
      //std::filesystem::file_time_type this_write = filesystem::last_write_time( databasefile );
      
      if( this_write <= last_write )
      {
        //cout << "Will NOT update from database" << endl;
        std::unique_lock<std::mutex> lock(  g_watch_db_mutex );
        g_update_cv.wait_for( lock, std::chrono::seconds(5) );
        if( !g_keep_watching_db )
          break;
        continue;
      }
        
      last_write = this_write;
      //cout << "Will update from database" << endl;
    }catch( std::exception &e )
    {
      cerr << "Caught exception from last_write_time: " << e.what() << endl;
      std::unique_lock<std::mutex> lock(  g_watch_db_mutex );
      g_update_cv.wait_for( lock, std::chrono::seconds(5) );
      if( !g_keep_watching_db )
        break;
      continue;
    }
    
    
    deque<tuple<WDateTime,int,bool>> new_oxygen, new_heartrate;  //<time,value,moving>"
    //deque<tuple<string,int,bool>> new_oxygen, new_heartrate;  //<time,value,moving>"
    deque<DbStatus> new_statuses;
    std::map<WDateTime,OwletReading> new_readings;
    
    
    try
    {
      dbo::Transaction transaction{session};
      auto new_points = session.find<DbOxygen>()
                      .where( "id > ?" ).bind(last_oxygen_index)
                      .orderBy("id DESC")
                      .limit(g_max_data_entries)
                      .resultList();
      
      //cout << "The oxygen query returned " << new_points.size() << " rows" << endl;
      
      for( const auto &point : new_points )
      {
        if( iteration_num )
        {
          const auto local_date_str = point->local_date;
          cout << "New Oxygen Point " << local_date_str << ": "
               << point->value << ", movement=" << point->movement << endl;
        }
        const WDateTime utc = utcDateTimeFromStr(point->utc_date);
        last_oxygen_index = std::max( last_oxygen_index, static_cast<int>(point.id()) );
        new_oxygen.push_front( {utc,point->value,static_cast<bool>(point->movement)} );
        
        auto pos = new_readings.find(utc);
        if( pos == end(new_readings) )
          pos = new_readings.insert( {utc,OwletReading{}} ).first;
        pos->second.utc_time = utc;
        pos->second.oxygen = point->value;
        pos->second.movement = point->movement;
      }
    }catch( dbo::Exception &e )
    {
      cerr << "Exception getting new Oxygen points: " << e.what() << endl;
    }
    
    try
    {
      dbo::Transaction transaction{session};
      auto new_points = session.find<DbHeartrate>()
                      .where( "id > ?" ).bind(last_heartrate_index)
                      .orderBy("id DESC")
                      .limit(g_max_data_entries)
                      .resultList();
      for( const auto &point : new_points )
      {
        if( iteration_num )
        {
          const auto &local_date_str = point->local_date;
          cout << "New HeartRate Point " << local_date_str << ": "
               << point->value << ", movement=" << point->movement << endl;
        }//if( iteration_num )
        
        const WDateTime utc = utcDateTimeFromStr(point->utc_date);
        
        last_heartrate_index = std::max( last_heartrate_index, static_cast<int>(point.id()) );
        new_heartrate.push_front( {utc,point->value,static_cast<bool>(point->movement)} );
        
        auto pos = new_readings.find(utc);
        if( pos == end(new_readings) )
          pos = new_readings.insert( {utc,OwletReading{}} ).first;
        pos->second.utc_time = utc;
        pos->second.heartrate = point->value;
        pos->second.movement = point->movement;
      }
    }catch( dbo::Exception &e )
    {
      cerr << "Exception getting new Oxygen points: " << e.what() << endl;
    }
    
    
    try
    {
      dbo::Transaction transaction{session};
      auto new_points = session.find<DbStatus>()
                      .where( "id > ?" ).bind(last_status_index)
                      .orderBy("id DESC")
                      .limit(g_max_data_entries)
                      .resultList();
      for( const auto &point : new_points )
      {
        if( iteration_num )
          cout << "New Status Point " << point->local_date << ": "
               << ", movement=" << point->movement
               << ", sock_off=" << point->sock_off
               << ", base_station=" << point->base_station
               << ", sock_connection=" << point->sock_connection
               << ", battery=" << point->battery
               << endl;
        last_status_index = std::max( last_status_index, static_cast<int>(point.id()) );
        new_statuses.push_front( *point );
        
        const WDateTime utc = utcDateTimeFromStr(point->utc_date);
        auto pos = new_readings.find(utc);
        if( pos == end(new_readings) )
          pos = new_readings.insert( {utc,OwletReading{}} ).first;
        pos->second.utc_time        = utc;
        pos->second.movement        = point->movement;
        pos->second.sock_off        = point->sock_off;
        pos->second.base_station    = point->base_station;
        pos->second.sock_connection = point->sock_connection;
        pos->second.battery         = point->battery;
      }//for( const auto &point : new_points )
    }catch( dbo::Exception &e )
    {
      cerr << "Exception getting new Oxygen points: " << e.what() << endl;
    }
    
    //cout << "Got " << new_oxygen.size() << " new oxygens" << endl;
    size_t num_readings_before = 0, num_readings_after = 0;
    
    
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      g_oxygen_values.insert( end(g_oxygen_values), begin(new_oxygen), end(new_oxygen) );
      g_heartrate_values.insert( end(g_heartrate_values), begin(new_heartrate), end(new_heartrate) );
      g_statuses.insert( end(g_statuses), begin(new_statuses), end(new_statuses) );
      
      num_readings_before = g_readings.size();
      for( const auto &iter : new_readings )
      {
        const auto &thispoint = iter.second;
        
        //Lets keep the chart from connecting lines between points greater than two minutes apart
        //  but where there are statuses within those few minutes - this is a hack, but whatever
        if( (prev_heart.isValid() && std::abs(prev_heart.secsTo(thispoint.utc_time)) > g_nsec_no_data)
             || (prev_o2.isValid() && std::abs(prev_o2.secsTo(thispoint.utc_time)) > g_nsec_no_data) )
        {
          OwletReading dummy;
          dummy.utc_time = thispoint.utc_time.addSecs(-1);
          g_readings.push_back( dummy );
        }
        
        if( thispoint.heartrate > 0 )
          prev_heart = thispoint.utc_time;
        if( thispoint.oxygen > 0 )
          prev_o2 = thispoint.utc_time;
        
        g_readings.push_back( thispoint );
      }//for( const auto &iter : new_readings )
      
      num_readings_after = g_readings.size();
      
      if( g_oxygen_values.size() > (g_max_data_entries+100) )
        g_oxygen_values.erase(begin(g_oxygen_values), begin(g_oxygen_values)+g_max_data_entries );
      if( g_heartrate_values.size() > (g_max_data_entries+100) )
        g_heartrate_values.erase(begin(g_heartrate_values), begin(g_heartrate_values)+g_max_data_entries );
      if( g_statuses.size() > (g_max_data_entries+100) )
        g_statuses.erase(begin(g_statuses), begin(g_statuses)+g_max_data_entries );
      
      // \TODO: implement some sort of condition variable based mechanism to delete old points.
      //if( g_readings.size() > (g_max_data_entries+100) )
      //  g_readings.erase(begin(g_readings), begin(g_readings)+g_max_data_entries );
    }//end lock on g_data_mutex
    
    if( num_readings_before != num_readings_after )
      OwletWebApp::addedData( num_readings_before, num_readings_after );
    
    
    map<WDateTime,tuple<WDateTime,int,int,int>> data;
    for( const auto &v : new_oxygen )
    {
      try
      {
        const WDateTime &date = get<0>(v);
        if( date.isNull() || !date.isValid() )
          throw runtime_error( "Failed to convert '" + date.toString("yyyy-MM-dd hh:mm:ss",false).toUTF8() + "'" );
        
        auto &val = data[date];
        get<0>(val) = date;
        get<1>(val) = get<1>(v);
        get<3>(val) = get<2>(v);
      }catch( std::exception &e )
      {
        cerr << "Caught exception updating new_oxygen: " << e.what() << endl;
      }
    }//for( const auto &v : new_oxygen )
    
    for( const auto &v : new_heartrate )
    {
      try
      {
        const WDateTime &date = get<0>(v);
        if( date.isNull() || !date.isValid() )
          throw runtime_error( "Invalid heartrate time" );
        
        auto &val = data[date];
        get<0>(val) = date;
        get<2>(val) = get<1>(v);
        get<3>(val) = get<2>(v);
      }catch( std::exception &e )
      {
        cerr << "Caught exception updating new_heartrate: " << e.what() << endl;
      }
    }//for( const auto &v : new_heartrate )
  
    vector<tuple<WDateTime,int,int,int>> sorted_data;
    sorted_data.reserve( data.size() );
    for( auto &vp : data )
      sorted_data.push_back( std::move(vp.second) );
    
    OwletWebApp::updateData( sorted_data );
    OwletWebApp::updateStatuses( new_statuses );
    
    
    ++iteration_num;
    
    std::unique_lock<std::mutex> lock(  g_watch_db_mutex );
    
    g_update_cv.wait_for( lock, std::chrono::seconds(5) );
      
    if( !g_keep_watching_db )
      break;
  };//while( true )
}//void look_for_db_changes()
