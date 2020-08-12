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
class SettingsDialog;
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

/**
 - Add in a short time interval "ultra low" alarm
 - Fix settings to be larger text on tablet; maybe make settings on popup
 - Maybe add in a day/night alarming profile
 - Start using Wt chart
 - Make Alarmer smarter when setting new settins
 */

class OwletWebApp : public Wt::WApplication
{
public:
  OwletWebApp(const Wt::WEnvironment& env);
  
  void updateDataToClient( const std::vector<std::tuple<Wt::WDateTime,int,int,int>> &data );
  void updateOxygenToClient( const Wt::WDateTime &utc, const int value );
  void updateHeartRateToClient( const Wt::WDateTime &utc, const int value );
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
  
  void checkInitialAutoAudioPlay();
  void audioPlayPopup();
  
  bool isShowingFiveMinuteAvrg();
  void showFiveMinuteAvrgData();
  void showIndividualPointsData();
  
  static void call_for_all( void(OwletWebApp::*mfcn)(void) );
  
  static void start_oxygen_alarms();
  
  static void end_oxygen_alarms();
  
  static void start_low_heartrate_alarms();
  
  
  static void stop_low_heartrate_alarms();
  
  
  static void start_high_heartrate_alarms();
  
  
  static void stop_high_heartrate_alarms();
  
  
  static void start_sock_off_alarms();
  static void stop_sock_off_alarms();
  
  void doInitialAlarming();
  
  void oxygenSnoozed();
  void lowHeartrateSnoozed();
  void highHeartrateSnoozed();
  void sockOffSnoozed();
  
  static void processDataForAlarming( const std::vector<std::tuple<Wt::WDateTime,int,int,int>> &data );
  static void processStatusForAlarming( const std::deque<DbStatus> &new_statuses );
  
  static void updateStatuses( const std::deque<DbStatus> &new_statuses );
  
  static void updateData( const std::vector<std::tuple<Wt::WDateTime,int,int,int>> &data );
  
  static void addedData( const size_t num_readings_before, const size_t num_readings_after );
  
  static void alarm_thresholds_updated();
  
  void set_error( std::string msg );
  
  
  static void parse_ini();

  
  static void init_globals();
  
  
  static void save_ini();

  static int get_config_value( const char *key );
  
  static void set_config_value( const char *key, const int value  );
  
  
private:
  Wt::WGridLayout *m_layout;
  
  Wt::WText *m_status;
  
  OwletChart *m_chart;
  
  Wt::WContainerWidget *m_oxygen_disp;
  Wt::WText *m_current_oxygen;
  
  Wt::WContainerWidget *m_heartrate_disp;
  Wt::WText *m_current_heartrate;

  Wt::WMessageBox *m_oxygen_mb;
  Wt::WMessageBox *m_low_heartrate_mb;
  Wt::WMessageBox *m_high_heartrate_mb;
  Wt::WMessageBox *m_sock_off_mb;
  
  SettingsDialog *m_settings;
  
  std::unique_ptr<Wt::JSignal<bool>> m_auto_play_test;
  
  static const char * const sm_ini_config_filename;
  
public:
  static std::mutex sm_ini_mutex;
  static boost::property_tree::ptree sm_ini;
  
  static Alarmer sm_oxygen_alarm;
  static Alarmer sm_heartrate_low_alarm;
  static Alarmer sm_heartrate_high_alarm;
  static Alarmer sm_sock_off_alarm;
};//OwletWebApp


#endif  //OwletWebApp_h
