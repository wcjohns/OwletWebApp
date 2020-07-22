#ifndef OwletWebApp_h
#define OwletWebApp_h

#include <tuple>
#include <mutex>
#include <vector>
#include <tuple>
#include <string>
#include <deque>

#include <boost/property_tree/ini_parser.hpp>

#include <Wt/WApplication.h>

#include "Alarmer.h"
#include "Database.h"

//Forward declarations
class OwletChart;
namespace Wt
{
  class WText;
  class WSpinBox;
  class WCheckBox;
  class WPushButton;
  class WGridLayout;
  class WMessageBox;
  class WEnvironment;
}//namespace Wt


class OwletWebApp : public Wt::WApplication
{
public:
  OwletWebApp(const Wt::WEnvironment& env);

  void toggleLines();
  
  
  void updateDataToClient( const std::vector<std::tuple<std::string,int,int,int>> &data );
  void updateStatusToClient( const DbStatus status );
  
  void startOxygenAlarm();
  void startLowHeartRateAlarm();
  void startHighHeartRateAlarm();
  void startSockOffAlarm();
  
  void snoozeOxygenAlarm();
  void snoozeLowHeartRateAlarm();
  void snoozeHighHeartRateAlarm();
  void snoozeSockOffAlarm();
  
  void oxygenAlarmEnded();
  void lowHeartRateAlarmEnded();
  void highHeartRateAlarmEnded();
  void sockOffAlarmEnded();
  
  void audioPlayPopup();
  
  static void call_for_all( void(OwletWebApp::*mfcn)(void) );
  
  static void start_oxygen_alarms();
  
  static void end_oxygen_alarms();
  
  static void start_low_heartrate_alarms();
  
  
  static void stop_low_heartrate_alarms();
  
  
  static void start_high_heartrate_alarms();
  
  
  static void stop_high_heartrate_alarms();
  
  
  static void start_sock_off_alarms();
  static void stop_sock_off_alarms();
  
  
  void oxygenSnoozed();
  void lowHeartrateSnoozed();
  void highHeartrateSnoozed();
  void sockOffSnoozed();
  
  static void processDataForAlarming( const std::vector<std::tuple<std::string,int,int,int>> &data );
  static void processStatusForAlarming( const std::deque<DbStatus> &new_statuses );
  
  static void updateStatuses( const std::deque<DbStatus> &new_statuses );
  
  static void updateData( const std::vector<std::tuple<std::string,int,int,int>> &data );
  
  
  void set_error( std::string msg );
  
  void setThresholdsFromIniToGui();
  
  
  static void parse_ini();

  
  static void init_globals();
  
  
  static void save_ini();

  static int get_config_value( const char *key );
  
  static void set_config_value( const char *key, const int value  );
  
  
private:
  Wt::WGridLayout *m_layout;
  
  Wt::WContainerWidget *m_status_disp;
  Wt::WText *m_status;
  Wt::WText *m_status_time;
  
  OwletChart *m_chart;
  Wt::WCheckBox *m_show_oxygen;
  Wt::WCheckBox *m_show_heartrate;
  
  Wt::WContainerWidget *m_oxygen_disp;
  Wt::WText *m_current_oxygen;
  Wt::WText *m_current_oxygen_time;
  
  Wt::WContainerWidget *m_heartrate_disp;
  Wt::WText *m_current_heartrate;
  Wt::WText *m_current_heartrate_time;

  Wt::WMessageBox *m_oxygen_mb;
  Wt::WSpinBox *m_oxygen_limit;
  Wt::WSpinBox *m_oxygen_time_wait;
  
  Wt::WMessageBox *m_low_heartrate_mb;
  Wt::WSpinBox *m_low_heartrate_limit;
  
  Wt::WMessageBox *m_high_heartrate_mb;
  Wt::WSpinBox *m_hearrate_time_wait;
  Wt::WSpinBox *m_high_heartrate_limit;
  
  Wt::WMessageBox *m_sock_off_mb;
  Wt::WSpinBox *m_sock_off_wait;
  
  
  static std::mutex sm_ini_mutex;
  static boost::property_tree::ptree sm_ini;

  static const char * const sm_ini_config_filename;
  
  static Alarmer sm_oxygen_alarm;
  static Alarmer sm_heartrate_low_alarm;
  static Alarmer sm_heartrate_high_alarm;
  static Alarmer sm_sock_off_alarm;
};//OwletWebApp


#endif  //OwletWebApp_h