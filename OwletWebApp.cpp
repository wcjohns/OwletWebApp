
#include <iostream>


#include <Wt/WText.h>
#include <Wt/WImage.h>
#include <Wt/WAudio.h>
#include <Wt/WTimer.h>
#include <Wt/WLabel.h>
#include <Wt/WServer.h>
#include <Wt/WSpinBox.h>
#include <Wt/WLineEdit.h>
#include <Wt/WDateTime.h>
#include <Wt/WCheckBox.h>
#include <Wt/WIOService.h>
#include <Wt/WMessageBox.h>
#include <Wt/WGridLayout.h>
#include <Wt/WPushButton.h>
//#include <Wt/WMediaPlayer.h>
#include <Wt/WEnvironment.h>
#include <Wt/WApplication.h>
#include <Wt/WStringStream.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WBootstrapTheme.h>
#include <Wt/WContainerWidget.h>


#include "OwletWebApp.h"
#include "OwletChart.h"

using namespace std;
using namespace Wt;

std::mutex OwletWebApp::sm_ini_mutex;
boost::property_tree::ptree OwletWebApp::sm_ini;
const char * const OwletWebApp::sm_ini_config_filename = "config/settings.ini";

Alarmer OwletWebApp::sm_oxygen_alarm;
Alarmer OwletWebApp::sm_heartrate_low_alarm;
Alarmer OwletWebApp::sm_heartrate_high_alarm;
Alarmer OwletWebApp::sm_sock_off_alarm;


OwletWebApp::OwletWebApp(const Wt::WEnvironment& env)
: WApplication(env),
  m_oxygen_mb( nullptr ),
  m_low_heartrate_mb( nullptr ),
  m_high_heartrate_mb( nullptr ),
  m_sock_off_mb( nullptr )
{
  enableUpdates();
  setTitle("OwletWebApp");
  
  //auto theme = make_shared<WBootstrapTheme>();
  //theme->setVersion( BootstrapVersion::v3 );
  //theme->setFormControlStyleEnabled( false );
  //theme->setResponsive( false );
  //setTheme( theme );
  
  //setCssTheme("polished");
  
  WApplication::instance()->useStyleSheet( "assets/css/OwletWebApp.css" );
  
  m_layout = root()->setLayout( make_unique<WGridLayout>() );
  
  m_oxygen_disp = m_layout->addWidget( std::make_unique<WContainerWidget>(), 0, 0, AlignmentFlag::Middle | AlignmentFlag::Center );
  m_oxygen_disp->addStyleClass( "CurrentOxygen" );
  
  auto icon = m_oxygen_disp->addWidget( make_unique<WImage>( WLink("assets/images/noun_Lungs_2911021.svg") ) );
  icon->setHeight( WLength(25,LengthUnit::Pixel) );
  icon->addStyleClass( "InfoIcon" );
  //auto label = m_oxygen_disp->addWidget( make_unique<WLabel>("Current Oxygen: ") );
  m_current_oxygen = m_oxygen_disp->addWidget( make_unique<WText>("--") );
  m_current_oxygen->addStyleClass( "CurrentValueTxt" );
  auto postfix = m_oxygen_disp->addWidget( make_unique<WText>("%") );
  postfix->addStyleClass( "CurrentValueUnits" );
  m_current_oxygen_time = m_oxygen_disp->addWidget( make_unique<WText>("") );
  m_current_oxygen_time->addStyleClass( "CurrentValueTime" );
  
  
  m_heartrate_disp = m_layout->addWidget( std::make_unique<WContainerWidget>(), 0, 1, AlignmentFlag::Middle | AlignmentFlag::Center );
  m_heartrate_disp->addStyleClass( "CurrentHeart" );
  
  icon = m_heartrate_disp->addWidget( make_unique<WImage>( WLink("assets/images/noun_Heart_3432419.svg") ) );
  icon->setHeight( WLength(25,LengthUnit::Pixel) );
  icon->addStyleClass( "InfoIcon" );
  m_current_heartrate = m_heartrate_disp->addWidget( make_unique<WText>("--") );
  m_current_heartrate->addStyleClass( "CurrentValueTxt" );
  postfix = m_heartrate_disp->addWidget( make_unique<WText>("&nbsp;BPM") );
  postfix->addStyleClass( "CurrentValueUnits" );
  m_current_heartrate_time = m_heartrate_disp->addWidget( make_unique<WText>("") );
  m_current_heartrate_time->addStyleClass( "CurrentValueTime" );
  
  
  m_status_disp = m_layout->addWidget( std::make_unique<WContainerWidget>(), 0, 2, AlignmentFlag::Middle | AlignmentFlag::Center );
  m_status_disp->addStyleClass( "Status" );
  icon = m_status_disp->addWidget( make_unique<WImage>( WLink("assets/images/noun_Info_2455956.svg") ) );
  icon->setHeight( WLength(25,LengthUnit::Pixel) );
  icon->addStyleClass( "InfoIcon" );
  m_status = m_status_disp->addWidget( std::make_unique<WText>("--") );
  m_status->addStyleClass( "StatusTxt" );
  m_status_time = m_status_disp->addWidget( std::make_unique<WText>("") );
  m_status_time->addStyleClass( "CurrentValueTime" );
  
  m_chart = m_layout->addWidget( std::make_unique<OwletChart>(true, false), 1, 0, 1, 3 );
  
  // \TODO: set height based off actual screen size
  //m_chart->setMaximumSize( WLength::Auto, WLength(400,Wt::LengthUnit::Pixel) );
  
  auto showoptions = m_layout->addWidget( make_unique<WContainerWidget>(), 2, 0, 1, 3 );
  m_show_oxygen = showoptions->addWidget( make_unique<WCheckBox>("Oxygen") );
  m_show_oxygen->setInline( false );
  
  //m_show_oxygen = m_layout->addWidget( make_unique<WCheckBox>("Oxygen"), 0, 1, AlignmentFlag::Left );
  m_show_oxygen->setChecked( true );
  m_show_oxygen->changed().connect( this, &OwletWebApp::toggleLines );
  
  m_show_heartrate = showoptions->addWidget( make_unique<WCheckBox>("HeartRate") );
  m_show_heartrate->setInline( false );
  //m_show_heartrate = m_layout->addWidget( make_unique<WCheckBox>("HeartRate"), 1, 1, AlignmentFlag::Left );
  m_show_heartrate->setChecked( false );
  m_show_heartrate->changed().connect( this, &OwletWebApp::toggleLines );
  
  //"Zoom: click-drag, Pan: shift-click-drag, Restore: double-click"
  
  
  auto oxygen_alarm_row = m_layout->addWidget( make_unique<WContainerWidget>(), 3, 0, 1, 3 );
  
  auto label = oxygen_alarm_row->addWidget( make_unique<WLabel>("Lower Oxygen Limit:&nbsp;") );
  m_oxygen_limit = oxygen_alarm_row->addWidget( make_unique<WSpinBox>() );
  m_oxygen_limit->setRange( 80, 100 );
  m_oxygen_limit->setSingleStep( 1 );
  //m_oxygen_limit->setNativeControl( true );
  m_oxygen_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "OxygenThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( sm_oxygen_alarm.m_mutex );
      sm_oxygen_alarm.m_threshold = value;
    }
  });
  
  
  label = oxygen_alarm_row->addWidget( make_unique<WLabel>("&nbsp;&nbsp;Seconds under limit before alarm:&nbsp;") );
  m_oxygen_time_wait = oxygen_alarm_row->addWidget( make_unique<WSpinBox>() );
  m_oxygen_time_wait->setRange( 0, 600 );
  m_oxygen_time_wait->setSingleStep( 1 );
  m_oxygen_time_wait->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "BelowOxygenThresholdSeconds", value );
    
    {
      std::unique_lock<std::mutex> lock( sm_oxygen_alarm.m_mutex );
      sm_oxygen_alarm.m_deviation_seconds = value;
      
      // \TODO: if sm_oxygen_alarm.m_alarm_wait_timer is currently set - make it shorter
    }
  });

  
  auto low_heart_row = m_layout->addWidget( make_unique<WContainerWidget>(), 4, 0, 1, 3 );
  
  label = low_heart_row->addWidget( make_unique<WLabel>("Lower Heartrate Limit:&nbsp;") );
  m_low_heartrate_limit = low_heart_row->addWidget( make_unique<WSpinBox>() );
  m_low_heartrate_limit->setRange( 20, 150 );
  m_low_heartrate_limit->setSingleStep( 1 );
  m_low_heartrate_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "HeartRateLowerThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( sm_heartrate_low_alarm.m_mutex );
      sm_heartrate_low_alarm.m_threshold = value;
    }
  });
  
  
  auto high_heart_row = m_layout->addWidget( make_unique<WContainerWidget>(), 5, 0, 1, 3 );
  
  label = high_heart_row->addWidget( make_unique<WLabel>("Upper Heartrate Limit:&nbsp;") );
  m_high_heartrate_limit = high_heart_row->addWidget( make_unique<WSpinBox>() );
  m_high_heartrate_limit->setRange( 40, 300 );
  m_high_heartrate_limit->setSingleStep( 1 );
  m_high_heartrate_limit->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "HeartRateUpperThreshold", value );
    
    {
      std::unique_lock<std::mutex> lock( sm_heartrate_low_alarm.m_mutex );
      sm_heartrate_high_alarm.m_threshold = value;
    }
  });
  
  label = high_heart_row->addWidget( make_unique<WLabel>("&nbsp;&nbsp;Seconds outside heart rate before alarming:&nbsp;") );
  
  m_hearrate_time_wait = high_heart_row->addWidget( make_unique<WSpinBox>() );
  m_hearrate_time_wait->setRange( 0, 600 );
  m_hearrate_time_wait->setSingleStep( 1 );
  m_hearrate_time_wait->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "HeartRateThresholdSeconds", value );
    
    {
      std::unique_lock<std::mutex> lock( sm_heartrate_low_alarm.m_mutex );
      sm_heartrate_high_alarm.m_deviation_seconds = value;
    }
  });
  
  
  auto sock_off_row = m_layout->addWidget( make_unique<WContainerWidget>(), 6, 0, 1, 3 );
  label = sock_off_row->addWidget( make_unique<WLabel>("Seconds with sock off allowed:&nbsp;") );
  m_sock_off_wait = sock_off_row->addWidget( make_unique<WSpinBox>() );
  m_sock_off_wait->setRange( -1, 2400 );
  m_sock_off_wait->setSingleStep( 1 );
  m_sock_off_wait->valueChanged().connect( []( const int value ){
    OwletWebApp::set_config_value( "SockOffSeconds", value );
    
    sm_sock_off_alarm.set_enabled( (value>=0) );
    if( value >= 0 )
    {
      std::unique_lock<std::mutex> lock( sm_heartrate_low_alarm.m_mutex );
      sm_sock_off_alarm.m_deviation_seconds = value;
    }
  });
  
  
  string iframe_js = "var iframe = document.createElement('iframe');"
  "iframe.src = 'assets/audio/silence.mp3';"
  "iframe.style.display = 'none';"
  "iframe.allow = 'autoplay';"
  "iframe.id = 'audio';"
  "document.body.appendChild(iframe);";
  doJavaScript( iframe_js );
  
  //const char *iframetxt = "'<iframe src=\"assets/audio/250-milliseconds-of-silence.mp3\" allow=\"autoplay\" id=\"audio\" style=\"display: none\"></iframe>'";
  //m_layout->addWidget( , m_layout->rowCount(), 0 );
  //domRoot()->addChild( make_unique<WText>(iframetxt,TextFormat::UnsafeXHTML) );
  
  
  setThresholdsFromIniToGui();
  
  m_layout->setRowStretch( 1, 1 );
  //m_layout->setColumnStretch( 0, 1 );
  
  
  if( sm_oxygen_alarm.currently_alarming() )
    startOxygenAlarm();
  else if( sm_oxygen_alarm.currently_snoozed() )
    snoozeOxygenAlarm();
  
  if( sm_heartrate_low_alarm.currently_alarming() )
    startLowHeartRateAlarm();
  else if( sm_heartrate_low_alarm.currently_snoozed() )
    snoozeLowHeartRateAlarm();
  
  if( sm_heartrate_high_alarm.currently_alarming() )
    startHighHeartRateAlarm();
  else if( sm_heartrate_high_alarm.currently_snoozed() )
    snoozeHighHeartRateAlarm();
  
  if( sm_sock_off_alarm.currently_alarming() )
    startSockOffAlarm();
  else if( sm_sock_off_alarm.currently_snoozed() )
    snoozeSockOffAlarm();
  
  
  //Now lets see if we have fairly recent values and status, and if so, update the users screen
  unique_ptr<DbStatus> latest_status;
  int last_oxygen = 0, last_heartrate = 0;
  Wt::WDateTime last_oxygen_time, last_heartrate_time;
  
  {//begin lock on g_data_mutex
    std::lock_guard<mutex> data_lock( g_data_mutex );
    const auto currentUtc = WDateTime::currentDateTime();
    if( g_statuses.size()  )
    {
      const auto last_utc = utcDateTimeFromStr(g_statuses.back().utc_date);
      if( abs(last_utc.secsTo(currentUtc)) < 10*60 )
        latest_status = make_unique<DbStatus>( g_statuses.back() );
    }
      
    if( g_oxygen_values.size() )
    {
      const auto last_utc = get<0>( g_oxygen_values.back() );
      if( abs(last_utc.secsTo(currentUtc)) < 2*60 )
      {
        last_oxygen_time = last_utc;
        last_oxygen = get<1>( g_oxygen_values.back() );
      }
    }//if( g_oxygen_values.size() )
    
    
    if( g_heartrate_values.size() )
    {
      const auto last_utc = get<0>( g_heartrate_values.back() );
      if( abs(last_utc.secsTo(currentUtc)) < 2*60 )
      {
        last_heartrate_time = last_utc;
        last_heartrate = get<1>( g_heartrate_values.back() );
      }
    }//if( g_oxygen_values.size() )
  }//end lock on g_data_mutex
  
  if( latest_status )
    updateStatusToClient( *latest_status );
  
  if( last_oxygen_time.isValid() && last_oxygen )
    updateOxygenToClient( last_oxygen_time, last_oxygen );
  
  if( last_heartrate_time.isValid() && last_heartrate )
    updateHeartRateToClient( last_heartrate_time, last_heartrate );
  
  
  
  // \TODO: only do this next call if audio cant auto-play
  WTimer::singleShot( std::chrono::milliseconds(1500), this, &OwletWebApp::audioPlayPopup );
}//OwletWebApp constructor



void OwletWebApp::toggleLines()
{
  m_layout->removeWidget(m_chart);
  m_chart = m_layout->addWidget( std::make_unique<OwletChart>(m_show_oxygen->isChecked(),
                                                              m_show_heartrate->isChecked()), 1, 0, 1, 3 );
}


void OwletWebApp::updateOxygenToClient( const Wt::WDateTime &last_oxygen_time, const int last_oxygen )
{
  m_current_oxygen->setText( std::to_string(last_oxygen) + "" );
  const auto now = WDateTime::currentDateTime();
  auto localdate = last_oxygen_time.addSecs( 60* wApp->environment().timeZoneOffset().count() );
  const char *format = (now.date() == last_oxygen_time.date()) ? "hh:mm:ss" : "ddd hh:mm:ss";
  m_current_oxygen_time->setText( localdate.toString(format) );
}//void updateOxygenToClient( const Wt::WDateTime &utc, const int value, const int moving )


void OwletWebApp::updateHeartRateToClient( const Wt::WDateTime &last_heartrate_time, const int last_heart )
{
  m_current_heartrate->setText( std::to_string(last_heart) + "" );
  const auto now = WDateTime::currentDateTime();
  auto localdate = last_heartrate_time.addSecs( 60* wApp->environment().timeZoneOffset().count() );
  const char *format = (now.date() == last_heartrate_time.date()) ? "hh:mm:ss" : "ddd hh:mm:ss";
  m_current_heartrate_time->setText( localdate.toString(format) );
}//void updateHeartRateToClient( const Wt::WDateTime &utc, const int value, const int moving )


void OwletWebApp::updateDataToClient( const vector<tuple<Wt::WDateTime,int,int,int>> &data )
{
  m_chart->updateData( data );
  
  int last_oxygen = 0, last_heart = 0;
  WDateTime last_oxygen_time, last_heartrate_time;
  for( const auto &val : data )
  {
    const WDateTime &datetime = get<0>(val);
    const int o2 = get<1>(val);
    const int heart = get<2>(val);
    //const int movement = get<3>(val.second);
    
    assert( datetime.isValid() & !datetime.isNull() );
    
    last_oxygen = (o2 ? o2 : last_oxygen);
    last_oxygen_time = (o2 ? datetime  : last_oxygen_time);
    
    last_heart = (heart ? heart : last_heart);
    last_heartrate_time = heart ? datetime : last_heartrate_time;
  }//for( const auto &val : data )
  
  if( last_oxygen_time.isValid() && last_oxygen )
    updateOxygenToClient(last_oxygen_time, last_oxygen );
  
  if( last_heartrate_time.isValid() && !last_heartrate_time.isNull() && last_heart )
    updateHeartRateToClient( last_heartrate_time, last_heart );
}//void updateDataToClient( const vector<tuple<string,int,int,int>> &data )


void OwletWebApp::updateStatusToClient( const DbStatus status )
{
  string txt; // = "<b>Status:</b> ";
  //if( sm_oxygen_alarm.currently_alarming() || sm_oxygen_alarm.currently_snoozed() )
  //  txt += "<b>Oxygen Low</b>&nbsp;&nbsp;";
  
  //if( sm_heartrate_low_alarm.currently_alarming() || sm_heartrate_low_alarm.currently_snoozed() )
  //  txt += "<b>Heart rate Low</b>&nbsp;&nbsp;";
  
  //if( sm_heartrate_high_alarm.currently_alarming() || sm_heartrate_high_alarm.currently_snoozed() )
  //  txt += "<b>Heart rate high</b>&nbsp;&nbsp;";
  
  if( sm_sock_off_alarm.currently_alarming() || sm_sock_off_alarm.currently_snoozed() )
    txt += "<b>Sock Off</b>";
  
  if( txt.empty() )
    m_status_disp->removeStyleClass( "Alarming" );
  else
    m_status_disp->addStyleClass( "Alarming" );
  
  if( status.movement )
    txt += string(txt.empty() ? "" : ",&nbsp;") + "Moving";
  if( status.sock_off )
    txt += string(txt.empty() ? "" : ",&nbsp;") + "Sock Off";
  if( !status.base_station )
    txt += string(txt.empty() ? "" : ",&nbsp;") + "Base Off";
  if( !status.sock_connection )
    txt += string(txt.empty() ? "" : ",&nbsp;") + "Sock Discon.";
  if( status.battery < 25 )
    txt += string(txt.empty() ? "" : ",&nbsp;") + "Batt. " + to_string(status.battery) +"%";
  if( txt.empty() )
    txt = "Normal";
  
  m_status->setText( txt );
  
  WDateTime datetime = utcDateTimeFromStr(status.utc_date);
  assert( datetime.isValid() && !datetime.isNull() );
  
  const auto now = WDateTime::currentDateTime();
  auto localdate = datetime.addSecs( 60 * wApp->environment().timeZoneOffset().count() );
  const char *format = (now.date() == datetime.date()) ? "hh:mm:ss" : "yyyy-MM-dd hh:mm:ss";
  m_status_time->setText( localdate.toString(format).toUTF8() );
}//void updateStatusToClient( const DbStatus status )




void OwletWebApp::startOxygenAlarm()
{
  if( m_oxygen_mb )
  {
    cerr << "Already alarming for oxygen" << endl;
    return;
  }
  
  m_oxygen_mb = root()->addChild(make_unique<WMessageBox>(
                  "Low Oxygen Saturation",
                  "<p>Oxygen levels are bellow threshold.</p>",
                   Icon::Critical, StandardButton::None ) );

  WPushButton *btn = m_oxygen_mb->addButton( "Dismiss", StandardButton::Ok );
  btn->clicked().connect( this, &OwletWebApp::oxygenSnoozed );


  //auto audio = m_oxygen_mb->contents()->addWidget( make_unique<WMediaPlayer>(MediaType::Audio) );
  //audio->addSource( MediaEncoding::MP3, "assets/audio/CheckOnAriMommy.mp3" );
  ////WInteractWidget *repeat = audio->button(MediaPlayerButtonId::RepeatOn);
  //audio->play();
  
  auto audio = m_oxygen_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->setOptions( PlayerOption::Autoplay | PlayerOption::Loop ); //PlayerOption::Controls
  audio->addSource( "assets/audio/CheckOnAriMommy.mp3" );
  audio->setPreloadMode( MediaPreloadMode::Auto );
  audio->play();
  //<audio id="player" autoplay loop>
  //    <source src="assets/audio/CheckOnAriMommy.mp3" type="audio/mp3">
  //</audio>
  
  
  m_oxygen_mb->setModal( true );
  m_oxygen_mb->show();
  
  if( !m_oxygen_disp->hasStyleClass( "Alarming" ) )
    m_oxygen_disp->addStyleClass( "Alarming" );
  
  cout << "Showing Oxygen alarm dialog" << endl;
}//void startOxygenAlarm()


void OwletWebApp::oxygenAlarmEnded()
{
  //Called when level has gone above threshold
  cerr << "Ending oxygen alarm" << endl;
  if( m_oxygen_mb )
  {
    m_oxygen_mb->hide();
    m_oxygen_mb->removeFromParent();
  }
  m_oxygen_mb = nullptr;
  
  // \TODO: clear status text or colors or whatever
  
  m_oxygen_disp->removeStyleClass( "Alarming" );
}//void oxygenAlarmEnded()


void OwletWebApp::snoozeOxygenAlarm()
{
  cerr << "Snoozing oxygen alarm" << endl;
  if( m_oxygen_mb )
  {
    m_oxygen_mb->accept();
    m_oxygen_mb->removeFromParent();
  }
  m_oxygen_mb = nullptr;
  
  /// \TODO: display some status text or change some colors or whatever
}//snoozeOxygenAlarm()


void OwletWebApp::snoozeLowHeartRateAlarm()
{
  cerr << "Snoozing low heartrate alarm" << endl;
  if( m_low_heartrate_mb )
  {
    m_low_heartrate_mb->accept();
    m_low_heartrate_mb->removeFromParent();
  }
  m_low_heartrate_mb = nullptr;
  
  /// \TODO: display some status text or change some colors or whatever
}//snoozeLowHeartRateAlarm()


void OwletWebApp::snoozeHighHeartRateAlarm()
{
  cerr << "Snoozing high heartrate alarm" << endl;
  if( m_high_heartrate_mb )
  {
    m_high_heartrate_mb->accept();
    m_high_heartrate_mb->removeFromParent();
  }
  m_high_heartrate_mb = nullptr;
  
  /// \TODO: display some status text or change some colors or whatever
}//snoozeHighHeartRateAlarm()


void OwletWebApp::startLowHeartRateAlarm()
{
  if( m_low_heartrate_mb )
  {
    cerr << "Already alarming for low heartrate" << endl;
    return;
  }
  
  m_low_heartrate_mb = root()->addChild(make_unique<WMessageBox>(
                  "Low Heart Rate",
                  "<p>Heart rate bellow threshold.</p>",
                   Icon::Critical, StandardButton::None) );

  WPushButton *btn = m_low_heartrate_mb->addButton( "Dismiss", StandardButton::Ok );
  btn->clicked().connect( this, &OwletWebApp::lowHeartrateSnoozed );

  auto audio = m_low_heartrate_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->addSource("assets/audio/CheckOnAriMommy.mp3");
  audio->setOptions( PlayerOption::Autoplay | PlayerOption::Loop ); //PlayerOption::Controls
  audio->setPreloadMode( MediaPreloadMode::Auto );
  audio->play();
  
  
  m_low_heartrate_mb->setModal( true );
  m_low_heartrate_mb->show();
  
  if( !m_heartrate_disp->hasStyleClass( "Alarming" ) )
    m_heartrate_disp->addStyleClass( "Alarming" );
}//void startLowHeartRateAlarm()


void OwletWebApp::lowHeartRateAlarmEnded()
{
  //Called when level has gone above threshold
  cerr << "Ending low heartrate alarm" << endl;
  if( m_low_heartrate_mb )
  {
    m_low_heartrate_mb->accept();
    m_low_heartrate_mb->removeFromParent();
  }
  m_low_heartrate_mb = nullptr;
  
  // \TODO: clear status text or colors or whatever
  m_heartrate_disp->removeStyleClass( "Alarming" );
}//void lowHeartRateAlarmEnded()


void OwletWebApp::startHighHeartRateAlarm()
{
  if( m_high_heartrate_mb )
  {
    cerr << "Already alarming for high heartrate" << endl;
    return;
  }
  
  m_high_heartrate_mb = root()->addChild(make_unique<WMessageBox>(
                  "High Heart Rate",
                  "<p>Heart rate above threshold.</p>",
                   Icon::Critical, StandardButton::None ) );

  WPushButton *btn = m_high_heartrate_mb->addButton( "Dismiss", StandardButton::Ok );
  btn->clicked().connect( this, &OwletWebApp::highHeartrateSnoozed );

  auto audio = m_high_heartrate_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->addSource("assets/audio/CheckOnAriMommy.mp3");
  audio->setOptions( PlayerOption::Autoplay | PlayerOption::Loop ); //PlayerOption::Controls
  audio->setPreloadMode( MediaPreloadMode::Auto );
  audio->play();
  
  m_high_heartrate_mb->setModal( true );
  m_high_heartrate_mb->show();
  
  if( !m_heartrate_disp->hasStyleClass( "Alarming" ) )
  m_heartrate_disp->addStyleClass( "Alarming" );
}//void startHighHeartRateAlarm()


void OwletWebApp::highHeartRateAlarmEnded()
{
  //Called when level has gone above threshold
  cerr << "Ending low heartrate alarm" << endl;
  if( m_high_heartrate_mb )
  {
    m_high_heartrate_mb->accept();
    m_high_heartrate_mb->removeFromParent();
  }
  m_high_heartrate_mb = nullptr;
  
  // \TODO: clear status text or colors or whatever
  
  m_heartrate_disp->removeStyleClass( "Alarming" );
}//void highHeartRateAlarmEnded()


void OwletWebApp::startSockOffAlarm()
{
  if( m_sock_off_mb )
  {
    cerr << "Already alarming for sock off" << endl;
    return;
  }
  
  m_sock_off_mb = root()->addChild(make_unique<WMessageBox>(
                  "Sock Off",
                  "<p>Sock needs to be repositioned.</p>",
                   Icon::Critical, StandardButton::None ) );

  WPushButton *btn = m_sock_off_mb->addButton( "Dismiss", StandardButton::Ok );
  btn->clicked().connect( this, &OwletWebApp::sockOffSnoozed );

  auto audio = m_sock_off_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->addSource("assets/audio/CheckOnAriMommy.mp3");
  audio->setOptions( PlayerOption::Autoplay | PlayerOption::Loop ); //PlayerOption::Controls
  audio->setPreloadMode( MediaPreloadMode::Auto );
  audio->play();
  
  m_sock_off_mb->setModal( true );
  m_sock_off_mb->show();
  
  if( !m_status->hasStyleClass("Alarming") )
    m_status->addStyleClass( "Alarming" );
}//void startSockOffAlarm()


void OwletWebApp::snoozeSockOffAlarm()
{
  cerr << "Snoozing sock off alarm" << endl;
  if( m_sock_off_mb )
  {
    m_sock_off_mb->accept();
    m_sock_off_mb->removeFromParent();
  }
  m_sock_off_mb = nullptr;
  
  /// \TODO: display some status text or change some colors or whatever
}//void snoozeSockOffAlarm()


void OwletWebApp::sockOffAlarmEnded()
{
  cerr << "Snoozing sock off alarm" << endl;
  if( m_sock_off_mb )
  {
    m_sock_off_mb->accept();
    m_sock_off_mb->removeFromParent();
  }
  m_sock_off_mb = nullptr;
  
  // \TODO: clear status text or colors or whatever
  m_status->removeStyleClass( "Alarming" );
}//void sockOffAlarmEnded()


void OwletWebApp::audioPlayPopup()
{
  auto message = root()->addChild(make_unique<WMessageBox>(
                  "Allow Audio Playback",
                  "<p>To enable audio alarming, please manually play this audio.</p>",
                   Icon::Warning, StandardButton::None ) );

  auto audio = message->contents()->addWidget( make_unique<WAudio>() );
  audio->setOptions( PlayerOption::Controls ); //PlayerOption::Loop
  audio->addSource( "assets/audio/CheckOnAriMommy.mp3" );
  audio->setPreloadMode( MediaPreloadMode::Auto );
  //audio->play();
  audio->ended().connect( [=](){
    message->accept();
    message->removeFromParent();
    
    // \TODO: test that audio can actually autoplay now
  });
  
  WPushButton *btn = message->addButton( "Skip", StandardButton::Ok );
  btn->clicked().connect( [=](){
    message->accept();
    message->removeFromParent();
  } );
  
  message->setModal( true );
  message->show();
}//void audioPlayPopup();


void OwletWebApp::call_for_all( void(OwletWebApp::*mfcn)(void) )
{
  auto server = WServer::instance();
  assert( server );
  
  server->postAll( [mfcn](){
    auto app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
    if( !app )
      return;
    (app->*mfcn)();
    app->triggerUpdate();
  });
}//call_for_all


void OwletWebApp::start_oxygen_alarms()
{
  cout << "Starting oxygen alarm" << endl;
  call_for_all( &OwletWebApp::startOxygenAlarm );
}//static void start_oxygen_alarms()


void OwletWebApp::end_oxygen_alarms()
{
  //Called when oxygen has gone above threshold
  cout << "Ending oxygen alarm" << endl;
  call_for_all( &OwletWebApp::oxygenAlarmEnded );
}//static void start_oxygen_alarms()


void OwletWebApp::start_low_heartrate_alarms()
{
  cout << "Starting low heartrate alarm" << endl;
  call_for_all( &OwletWebApp::startLowHeartRateAlarm );
}//static void start_low_heartrate_alarms()


void OwletWebApp::stop_low_heartrate_alarms()
{
  cout << "Stopping low heartrate alarm" << endl;
  call_for_all( &OwletWebApp::lowHeartRateAlarmEnded );
}//static void stop_low_heartrate_alarms()


void OwletWebApp::start_high_heartrate_alarms()
{
  cout << "Starting high heartrate alarm" << endl;
  call_for_all( &OwletWebApp::startHighHeartRateAlarm );
}//static void start_low_heartrate_alarms()


void OwletWebApp::stop_high_heartrate_alarms()
{
  cout << "Stopping high heart rate alarm" << endl;
  call_for_all( &OwletWebApp::highHeartRateAlarmEnded );
}//static void stop_high_heartrate_alarms()


void OwletWebApp::start_sock_off_alarms()
{
  cout << "Starting sock off alarm" << endl;
  call_for_all( &OwletWebApp::startSockOffAlarm );
}//void OwletWebApp::start_sock_off_alarms()


void OwletWebApp::stop_sock_off_alarms()
{
  cout << "Stopping sock off alarm" << endl;
  call_for_all( &OwletWebApp::sockOffAlarmEnded );
}//void stop_sock_off_alarms();


void OwletWebApp::oxygenSnoozed()
{
  
  sm_oxygen_alarm.snooze_alarm();
  call_for_all( &OwletWebApp::snoozeOxygenAlarm );
}//void oxygenSnoozed()

void OwletWebApp::lowHeartrateSnoozed()
{
  sm_heartrate_low_alarm.snooze_alarm();
  call_for_all( &OwletWebApp::snoozeLowHeartRateAlarm );
}//void lowHeartrateSnoozed()


void OwletWebApp::highHeartrateSnoozed()
{
  sm_heartrate_high_alarm.snooze_alarm();
  call_for_all( &OwletWebApp::snoozeHighHeartRateAlarm );
}//void highHeartrateSnoozed()


void OwletWebApp::sockOffSnoozed()
{
  sm_sock_off_alarm.snooze_alarm();
  call_for_all( &OwletWebApp::snoozeSockOffAlarm );
}//void highHeartrateSnoozed()


void OwletWebApp::processDataForAlarming( const vector<tuple<Wt::WDateTime,int,int,int>> &data )
{
  for( size_t i = 0; i < data.size(); ++i )
  {
    const auto &val = data[i];
    const bool last_data = ((i+1) == data.size());
    
    const WDateTime &datetime = get<0>(val);
    assert( datetime.isValid() & !datetime.isNull() );
    
    const int oxygen = get<1>(val);
    const int heart = get<2>(val);
    
    sm_oxygen_alarm.processData( datetime, oxygen, last_data );
    sm_heartrate_low_alarm.processData( datetime, heart, last_data );
    sm_heartrate_high_alarm.processData( datetime, heart, last_data );
  }//for( const auto &val : data )
}//static void processDataForAlarming( const vector<tuple<string,int,int,int>> &data )


void OwletWebApp::processStatusForAlarming( const std::deque<DbStatus> &data )
{
  for( size_t i = 0; i < data.size(); ++i )
  {
    const auto &val = data[i];
    const bool last_data = ((i+1) == data.size());
      
    const WDateTime &datetime = utcDateTimeFromStr(val.utc_date);
    assert( datetime.isValid() & !datetime.isNull() );
    
    sm_sock_off_alarm.processData( datetime, val.sock_off, last_data );
  }//for( const auto &val : data )
}//static void processStatusForAlarming( const std::deque<DbStatus> &new_statuses );


void OwletWebApp::updateStatuses( const deque<DbStatus> &new_statuses )
{
  if( new_statuses.empty() )
    return;
  
  const DbStatus most_recent = new_statuses.back();
  
  auto server = WServer::instance();
  if( !server )
  {
    cerr << "OwletWebApp::updateStatuses(): failed to get WServer - returning" << endl;
    return;
  }
  
  server->postAll( [most_recent](){
    auto app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
    if( !app )
    {
      cerr << "Got dead instance?" << endl;
      return;
    }
      
    app->updateStatusToClient( most_recent );
    app->triggerUpdate();
  });
  
  processStatusForAlarming( new_statuses );
}//updateStatuses( const deque<DbStatus> &new_statuses )



void OwletWebApp::updateData( const vector<tuple<Wt::WDateTime,int,int,int>> &data )
{
  auto server = WServer::instance();
  if( !server )
  {
    cerr << "OwletWebApp::updateData(): failed to get WServer - returning" << endl;
    return;
  }
  
  if( !data.empty() )
  {
    server->postAll( [data](){
      auto app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
      if( !app )
      {
        cerr << "Got dead instance?" << endl;
        return;
      }
      
      app->updateDataToClient( data );
      app->triggerUpdate();
    });
  }//if( server and have data )
  
  processDataForAlarming( data );
}//void updateData( const vector<tuple<string,int,int,int>> &data )


void OwletWebApp::addedData( const size_t num_readings_before, const size_t num_readings_after )
{
  assert( num_readings_before < num_readings_after );
  
  auto server = WServer::instance();
  if( !server )
  {
    cerr << "OwletWebApp::addedData(): failed to get WServer - returning" << endl;
    return;
  }

  server->postAll( [=](){
    auto app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
    if( !app )
    {
      cerr << "Got dead instance?" << endl;
      return;
    }
      
    app->m_chart->addedData( num_readings_before, num_readings_after );
    app->triggerUpdate();
  });
}//void addedData( const size_t num_readings_before, const size_t num_readings_after )


void OwletWebApp::set_error( string msg )
{
  
}

void OwletWebApp::setThresholdsFromIniToGui()
{
  int OxygenThreshold, BelowOxygenThresholdSeconds, HeartRateLowerThreshold;
  int HeartRateUpperThreshold, HeartRateThresholdSeconds, SnoozeSeconds;
  int SockOffSeconds;
  
  {//begin lock on sm_ini_mutex
    std::unique_lock<std::mutex> lock(  sm_ini_mutex );
    SnoozeSeconds = sm_ini.get<int>("SnoozeSeconds");
    OxygenThreshold = sm_ini.get<int>("OxygenThreshold");
    BelowOxygenThresholdSeconds = sm_ini.get<int>("BelowOxygenThresholdSeconds");
    HeartRateLowerThreshold = sm_ini.get<int>("HeartRateLowerThreshold");
    HeartRateUpperThreshold = sm_ini.get<int>("HeartRateUpperThreshold");
    HeartRateThresholdSeconds = sm_ini.get<int>("HeartRateThresholdSeconds");
    SockOffSeconds = sm_ini.get<int>("SockOffSeconds");
  }//end lock on sm_ini_mutex
  
  //Now set widgets...
  m_oxygen_limit->setValue( OxygenThreshold );
  m_oxygen_time_wait->setValue( BelowOxygenThresholdSeconds );
  m_low_heartrate_limit->setValue( HeartRateLowerThreshold );
  m_high_heartrate_limit->setValue( HeartRateUpperThreshold );
  m_hearrate_time_wait->setValue( HeartRateThresholdSeconds );
  m_sock_off_wait->setValue( SockOffSeconds );
}//setThresholdsFromIniToGui()


void OwletWebApp::parse_ini()
{
  std::unique_lock<std::mutex> lock(  sm_ini_mutex );
  boost::property_tree::ini_parser::read_ini( sm_ini_config_filename, sm_ini );
  
  sm_ini.get<int>("SnoozeSeconds");
  sm_ini.get<int>("OxygenThreshold");
  sm_ini.get<int>("BelowOxygenThresholdSeconds");
  sm_ini.get<int>("HeartRateLowerThreshold");
  sm_ini.get<int>("HeartRateUpperThreshold");
  sm_ini.get<int>("HeartRateThresholdSeconds");
  sm_ini.get<int>("SockOffSeconds");
}//void parse_ini()


void OwletWebApp::init_globals()
{
  int OxygenThreshold, BelowOxygenThresholdSeconds, SnoozeSeconds;
  int HeartRateLowerThreshold, HeartRateUpperThreshold, HeartRateThresholdSeconds;
  int SockOffSeconds;
  
  {//begin lock on sm_ini_mutex
    std::unique_lock<std::mutex> lock(  sm_ini_mutex );
    SnoozeSeconds = sm_ini.get<int>("SnoozeSeconds");
    OxygenThreshold = sm_ini.get<int>("OxygenThreshold");
    BelowOxygenThresholdSeconds = sm_ini.get<int>("BelowOxygenThresholdSeconds");
    HeartRateLowerThreshold = sm_ini.get<int>("HeartRateLowerThreshold");
    HeartRateUpperThreshold = sm_ini.get<int>("HeartRateUpperThreshold");
    HeartRateThresholdSeconds = sm_ini.get<int>("HeartRateThresholdSeconds");
    SockOffSeconds = sm_ini.get<int>("SockOffSeconds");
  }//end lock on sm_ini_mutex
  
  
  sm_oxygen_alarm.m_threshold = OxygenThreshold;
  sm_oxygen_alarm.m_deviation_seconds = BelowOxygenThresholdSeconds;
  sm_oxygen_alarm.m_snooze_seconds = SnoozeSeconds;
  sm_oxygen_alarm.m_lessthan = true;
  sm_oxygen_alarm.m_start_alarm = [](){ OwletWebApp::start_oxygen_alarms(); };
  sm_oxygen_alarm.m_stop_alarm = [](){ OwletWebApp::end_oxygen_alarms(); };
  
  
  sm_heartrate_low_alarm.m_threshold = HeartRateLowerThreshold;
  sm_heartrate_low_alarm.m_deviation_seconds = HeartRateThresholdSeconds;
  sm_heartrate_low_alarm.m_snooze_seconds = SnoozeSeconds;
  sm_heartrate_low_alarm.m_lessthan = true;
  sm_heartrate_low_alarm.m_start_alarm = [](){ OwletWebApp::start_low_heartrate_alarms(); };
  sm_heartrate_low_alarm.m_stop_alarm = [](){ OwletWebApp::stop_low_heartrate_alarms(); };
  
  
  sm_heartrate_high_alarm.m_threshold = HeartRateUpperThreshold;
  sm_heartrate_high_alarm.m_deviation_seconds = HeartRateThresholdSeconds;
  sm_heartrate_high_alarm.m_snooze_seconds = SnoozeSeconds;
  sm_heartrate_high_alarm.m_lessthan = false;
  sm_heartrate_high_alarm.m_start_alarm = [](){ OwletWebApp::start_high_heartrate_alarms(); };
  sm_heartrate_high_alarm.m_stop_alarm = [](){ OwletWebApp::stop_high_heartrate_alarms(); };
  
  
  sm_sock_off_alarm.m_threshold = 1;
  sm_sock_off_alarm.m_deviation_seconds = SockOffSeconds;
  sm_sock_off_alarm.m_snooze_seconds = SnoozeSeconds;
  sm_sock_off_alarm.m_lessthan = true;
  sm_sock_off_alarm.m_start_alarm = [](){ OwletWebApp::start_sock_off_alarms(); };
  sm_sock_off_alarm.m_stop_alarm = [](){ OwletWebApp::stop_sock_off_alarms(); };
  sm_sock_off_alarm.set_enabled( SockOffSeconds>=0 );
}//static void init_globals()


void OwletWebApp::save_ini()
{
  std::unique_lock<std::mutex> lock(  sm_ini_mutex );
  boost::property_tree::ini_parser::write_ini( sm_ini_config_filename, sm_ini );
}//void save_ini()

int OwletWebApp::get_config_value( const char *key )
{
  std::unique_lock<std::mutex> lock(  sm_ini_mutex );
  
  if( !sm_ini.count(key) )
    throw runtime_error( "Invalid INI key when retrieving: " + string(key) );
  
  return sm_ini.get<int>( key );
}

void OwletWebApp::set_config_value( const char *key, const int value  )
{
  {
    std::unique_lock<std::mutex> lock(  sm_ini_mutex );
    if( !sm_ini.count(key) )
      throw runtime_error( "Invalid INI key when setting: " + string(key) );
    sm_ini.put( key, value );
  }
  
  auto server = WServer::instance();
  assert( server );
  
  server->postAll( [=](){
    auto app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
    if( app )
    {
      app->setThresholdsFromIniToGui();
      app->triggerUpdate();
    }else
    {
      cerr << "Got dead instance in updateSettings?" << endl;
    }
  });
  
  server->ioService().post( [](){ save_ini(); } );
}//void set_config_value(...)


