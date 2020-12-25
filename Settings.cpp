#include <string>

#include <Wt/WAudio.h>
#include <Wt/WLabel.h>
#include <Wt/WTable.h>
#include <Wt/WSpinBox.h>
#include <Wt/WCheckBox.h>
#include <Wt/WGroupBox.h>
#include <Wt/WPushButton.h>

#include "Settings.h"
#include "OwletWebApp.h"

using namespace std;
using namespace Wt;

#define INLINE_JAVASCRIPT(...) #__VA_ARGS__

namespace
{
  WAnimation ns_animation( AnimationEffect::SlideInFromTop, TimingFunction::Linear, 250 );

  const std::string ns_resize_to_fit_on_screen_js = INLINE_JAVASCRIPT
  (
   function()
   {
     let el = this;
     try
     {
       const ws = Wt.WT.windowSize();
       const jel = $(el);
       
       if( (jel.height() + 16) > ws.y)
       {
         $('#' + el.id + ' > .modal-content').height(ws.y - 16);
         el.style.top = '8px';
         el.style.bottom = null;
       }
     }catch(err){
       console.log( "Caught: " + err );
       console.log( el );
     }
   }
   );
}// namespace



SettingsDialog::SettingsDialog()
  : Wt::WDialog()
{
  addStyleClass( "SettingsDialog" );
  
  m_settings = contents()->addWidget( std::make_unique<Settings>());
  contents()->setOverflow( Overflow::Auto, Orientation::Vertical );
  
  setModal( true );
  //setClosable( true );
  setTitleBarEnabled( false );
  raiseToFront();
  show();
  
  
  setJavaScriptMember( "fitOnScreen", ns_resize_to_fit_on_screen_js );
  doJavaScript( "setTimeout( function(){ " + jsRef() + ".fitOnScreen();}, 50);" );
  
  
  Wt::WPushButton *ok = footer()->addWidget( std::make_unique<Wt::WPushButton>("Ok") );
  ok->clicked().connect( [this](){ accept(); } );
}


Settings::Settings()
  : WContainerWidget(),
    m_fivemin_avrg( nullptr ),
    m_oxygen_limit( nullptr ),
    m_second_oxygen_limit( nullptr ),
    m_second_oxygen_disabled( nullptr ),
    m_oxygen_time_wait( nullptr ),
    m_low_heartrate_limit( nullptr ),
    m_hearrate_time_wait( nullptr ),
    m_high_heartrate_limit( nullptr ),
    m_sock_off_disabled( nullptr ),
    m_sock_off_wait( nullptr )
{
  addStyleClass( "Settings" );
  
  OwletWebApp *app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
  m_fivemin_avrg = addWidget( make_unique<WCheckBox>("Chart 5 Minute Averages") );
  m_fivemin_avrg->addStyleClass( "FiveMinAvrgCb" );
  m_fivemin_avrg->setChecked( (app && app->isShowingFiveMinuteAvrg()) );
  m_fivemin_avrg->checked().connect( app, &OwletWebApp::showFiveMinuteAvrgData );
  m_fivemin_avrg->unChecked().connect( app, &OwletWebApp::showIndividualPointsData );

  
  auto oxygen_alarm_row = addWidget( make_unique<WGroupBox>("Oxygen Alarm") );
  
  auto row = oxygen_alarm_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  
  auto label = row->addWidget( make_unique<WLabel>("Lower Value") );
  label->addStyleClass( "AlarmValueLabel" );
  m_oxygen_limit = row->addWidget( make_unique<WSpinBox>() );
  m_oxygen_limit->setRange( 80, 100 );
  m_oxygen_limit->setSingleStep( 1 );
  m_oxygen_limit->setInputMask( "99" );
  m_oxygen_limit->setAttributeValue( "inputmode", "numeric");
  m_oxygen_limit->setAttributeValue( "pattern", "[0-9][0-9]");
  //m_oxygen_limit->setSuffix( "%" );
  label->setBuddy( m_oxygen_limit );
  //m_oxygen_limit->setNativeControl( true );
  m_oxygen_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "OxygenThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_oxygen_alarm.m_mutex );
      OwletWebApp::sm_oxygen_alarm.m_threshold = value;
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  row = oxygen_alarm_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  label = row->addWidget( make_unique<WLabel>("Delay Time (s)") );
  label->addStyleClass( "AlarmValueLabel" );
  m_oxygen_time_wait = row->addWidget( make_unique<WSpinBox>() );
  m_oxygen_time_wait->setRange( 0, 600 );
  m_oxygen_time_wait->setSingleStep( 1 );
  m_oxygen_time_wait->setAttributeValue( "inputmode", "numeric");
  m_oxygen_time_wait->setAttributeValue( "pattern", "[0-9]*");
  //m_oxygen_time_wait->setSuffix( "s" );
  label->setBuddy( m_oxygen_time_wait );
  m_oxygen_time_wait->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "BelowOxygenThresholdSeconds", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_oxygen_alarm.m_mutex );
      OwletWebApp::sm_oxygen_alarm.m_deviation_seconds = value;
      
      // \TODO: if sm_oxygen_alarm.m_alarm_wait_timer is currently set - make it shorter
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  
  auto second_oxygen_alarm_row = addWidget( make_unique<WGroupBox>("Second Oxygen Alarm") );
  
  row = second_oxygen_alarm_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "EnableRow" );
  m_second_oxygen_disabled = row->addWidget( make_unique<WCheckBox>("Disabled") );
  const bool second_oxygen_enabled = OwletWebApp::sm_second_oxygen_alarm.enabled();
  m_second_oxygen_disabled->setChecked( !second_oxygen_enabled );
  
  row = second_oxygen_alarm_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  
  label = row->addWidget( make_unique<WLabel>("Lower Value") );
  label->addStyleClass( "AlarmValueLabel" );
  m_second_oxygen_limit = row->addWidget( make_unique<WSpinBox>() );
  m_second_oxygen_limit->setRange( 80, 100 );
  m_second_oxygen_limit->setSingleStep( 1 );
  m_second_oxygen_limit->setInputMask( "99" );
  m_second_oxygen_limit->setAttributeValue( "inputmode", "numeric");
  m_second_oxygen_limit->setAttributeValue( "pattern", "[0-9][0-9]");
  //m_second_oxygen_limit->setSuffix( "%" );
  label->setBuddy( m_second_oxygen_limit );
  //m_second_oxygen_limit->setNativeControl( true );
  m_second_oxygen_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "SecondOxygenThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_second_oxygen_alarm.m_mutex );
      OwletWebApp::sm_second_oxygen_alarm.m_threshold = value;
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  
  row->setHidden( !second_oxygen_enabled );
  m_second_oxygen_disabled->changed().connect( [this,row](){
    const bool alarmDisabled = m_second_oxygen_disabled->isChecked();
    row->setHidden( alarmDisabled, ns_animation );
    OwletWebApp::sm_second_oxygen_alarm.set_enabled( !alarmDisabled );
    OwletWebApp::set_config_value( "SecondOxygenDisabled", alarmDisabled );
    OwletWebApp::alarm_thresholds_updated();
    doJavaScript( "setTimeout( function(){ " + jsRef() + ".fitOnScreen();}, 10);" );
  } );
  
  
  WText *hint = second_oxygen_alarm_row->addWidget( make_unique<WText>("This alarm is for immediate ultra-low alarms") );
  hint->setInline( false );
  hint->addStyleClass( "SockAlarmHint" );
  
  
  
  auto heart_row = addWidget( make_unique<WGroupBox>("Heartrate Alarm:") );
  row = heart_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  
  label = row->addWidget( make_unique<WLabel>("Lower Value") );
  label->addStyleClass( "AlarmValueLabel" );
  m_low_heartrate_limit = row->addWidget( make_unique<WSpinBox>() );
  m_low_heartrate_limit->setRange( 20, 150 );
  m_low_heartrate_limit->setSingleStep( 1 );
  label->setBuddy( m_low_heartrate_limit );
  m_low_heartrate_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "HeartRateLowerThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
      OwletWebApp::sm_heartrate_low_alarm.m_threshold = value;
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  
  row = heart_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  
  
  label = row->addWidget( make_unique<WLabel>("Upper Value") );
  label->addStyleClass( "AlarmValueLabel" );
  m_high_heartrate_limit = row->addWidget( make_unique<WSpinBox>() );
  m_high_heartrate_limit->setRange( 40, 300 );
  m_high_heartrate_limit->setSingleStep( 1 );
  label->setBuddy( m_high_heartrate_limit );
  m_high_heartrate_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "HeartRateUpperThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
      OwletWebApp::sm_heartrate_high_alarm.m_threshold = value;
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  row = heart_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  
  label = row->addWidget( make_unique<WLabel>("Delay Time (s)") );
  label->addStyleClass( "AlarmValueLabel" );
  
  m_hearrate_time_wait = row->addWidget( make_unique<WSpinBox>() );
  m_hearrate_time_wait->setRange( 0, 600 );
  m_hearrate_time_wait->setSingleStep( 1 );
  m_hearrate_time_wait->setAttributeValue( "inputmode", "numeric");
  m_hearrate_time_wait->setAttributeValue( "pattern", "[0-9]*");
  //m_hearrate_time_wait->setSuffix( "s" );
  label->setBuddy( m_hearrate_time_wait );
  m_hearrate_time_wait->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "HeartRateThresholdSeconds", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
      OwletWebApp::sm_heartrate_high_alarm.m_deviation_seconds = value;
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  auto sock_off_row = addWidget( make_unique<WGroupBox>("Sock Off Alarm") );
  row = sock_off_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "EnableRow" );
  m_sock_off_disabled = row->addWidget( make_unique<WCheckBox>("Disabled") );
  const bool sock_off_enabled = OwletWebApp::sm_sock_off_alarm.enabled();
  m_sock_off_disabled->setChecked( !sock_off_enabled );
  
  
  row = sock_off_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  label = row->addWidget( make_unique<WLabel>("Time Allowed") );
  label->addStyleClass( "AlarmValueLabel" );
  m_sock_off_wait = row->addWidget( make_unique<WSpinBox>() );
  m_sock_off_wait->setRange( 0, 2400 );
  m_sock_off_wait->setSingleStep( 1 );
  //m_sock_off_wait->setSuffix( "s" );
  m_sock_off_wait->setAttributeValue( "inputmode", "numeric");
  //m_sock_off_wait->setAttributeValue( "pattern", "*[0-9]*");
  label->setBuddy( m_sock_off_wait );
  m_sock_off_wait->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "SockOffSeconds", value );
    
    {
      std::unique_lock<std::mutex> lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
      OwletWebApp::sm_sock_off_alarm.m_deviation_seconds = value;
    }
    OwletWebApp::alarm_thresholds_updated();
  });
  
  
  row->setHidden( !sock_off_enabled );
  m_sock_off_disabled->changed().connect( [this,row](){
    const bool alarmDisabled = m_sock_off_disabled->isChecked();
    
    row->setHidden( alarmDisabled, ns_animation );
    OwletWebApp::sm_sock_off_alarm.set_enabled( !alarmDisabled );
    OwletWebApp::set_config_value( "SockOffDisabled", alarmDisabled );
    OwletWebApp::alarm_thresholds_updated();
    doJavaScript( "setTimeout( function(){ " + jsRef() + ".fitOnScreen();}, 10);" );
  } );
  
  
  //hint = sock_off_row->addWidget( make_unique<WText>("(-1 disables sock off alert)") );
  //hint->setInline( false );
  //hint->addStyleClass( "SockAlarmHint" );
  

  auto audio_row = addWidget( make_unique<WGroupBox>("Audio Level Check:") );
  row = audio_row->addWidget( make_unique<WContainerWidget>() );
  row->addStyleClass( "AlarmRow" );
  

  auto audio = audio_row->addWidget( make_unique<WAudio>() );
  audio->setInline( false );
  audio->setOptions( PlayerOption::Controls ); //PlayerOption::Loop
  audio->addSource( "assets/audio/CheckOnAriMommy.mp3" );
  audio->setPreloadMode( MediaPreloadMode::Auto );
  
  
  setThresholdsFromIniToGui();
}//Settings constructor


Settings::~Settings()
{
  
}


void Settings::setThresholdsFromIniToGui()
{
  int OxygenThreshold, SecondOxygenThreshold, BelowOxygenThresholdSeconds, HeartRateLowerThreshold;
  int HeartRateUpperThreshold, HeartRateThresholdSeconds;
  int SockOffSeconds;
  bool SecondOxygenDisabled, SockOffDisabled;
  //int SnoozeSeconds;
  
  {//begin lock on sm_ini_mutex
    std::unique_lock<std::mutex> lock( OwletWebApp::sm_ini_mutex );
    //SnoozeSeconds = sm_ini.get<int>("SnoozeSeconds");
    OxygenThreshold = OwletWebApp::sm_ini.get<int>("OxygenThreshold");
    SecondOxygenThreshold = OwletWebApp::sm_ini.get<int>("SecondOxygenThreshold");
    BelowOxygenThresholdSeconds = OwletWebApp::sm_ini.get<int>("BelowOxygenThresholdSeconds");
    SecondOxygenDisabled = OwletWebApp::sm_ini.get<bool>("SecondOxygenDisabled");
    HeartRateLowerThreshold = OwletWebApp::sm_ini.get<int>("HeartRateLowerThreshold");
    HeartRateUpperThreshold = OwletWebApp::sm_ini.get<int>("HeartRateUpperThreshold");
    HeartRateThresholdSeconds = OwletWebApp::sm_ini.get<int>("HeartRateThresholdSeconds");
    SockOffSeconds = OwletWebApp::sm_ini.get<int>("SockOffSeconds");
    SockOffDisabled = OwletWebApp::sm_ini.get<bool>("SockOffDisabled");
  }//end lock on sm_ini_mutex
  
  //Now set widgets...
  m_oxygen_limit->setValue( OxygenThreshold );
  m_second_oxygen_limit->setValue( SecondOxygenThreshold );
  m_oxygen_time_wait->setValue( BelowOxygenThresholdSeconds );
  m_low_heartrate_limit->setValue( HeartRateLowerThreshold );
  m_high_heartrate_limit->setValue( HeartRateUpperThreshold );
  m_hearrate_time_wait->setValue( HeartRateThresholdSeconds );
  m_sock_off_wait->setValue( SockOffSeconds );
  
  
  m_second_oxygen_disabled->setChecked( SecondOxygenDisabled );
  if( m_second_oxygen_limit->parent() )
    m_second_oxygen_limit->parent()->setHidden( SecondOxygenDisabled ); //ns_animation will cause a double animation
    
  m_sock_off_disabled->setChecked( SockOffDisabled );
  if( m_sock_off_wait->parent() )
    m_sock_off_wait->parent()->setHidden( SockOffDisabled ); //ns_animation will cause a double animation
}//setThresholdsFromIniToGui()
