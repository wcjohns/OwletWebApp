
#include <iostream>


#include <Wt/WText.h>
#include <Wt/WImage.h>
#include <Wt/WAudio.h>
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
#include <Wt/WEnvironment.h>
#include <Wt/WApplication.h>
#include <Wt/WStringStream.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WBootstrapTheme.h>
#include <Wt/WContainerWidget.h>

#include "Settings.h"
#include "OwletChart.h"
#include "OwletWebApp.h"

using namespace std;
using namespace Wt;

#define INLINE_JAVASCRIPT(...) #__VA_ARGS__

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
  m_sock_off_mb( nullptr ),
  m_settings( nullptr )
{
  enableUpdates();
  setTitle("OwletWebApp");
  
  auto theme = make_shared<WBootstrapTheme>();
  theme->setVersion( BootstrapVersion::v3 );
  theme->setFormControlStyleEnabled( false );
  theme->setResponsive( false );
  setTheme( theme );
  
  //setCssTheme("polished");
  
  const char *fix_ios_js = INLINE_JAVASCRIPT(
    var t=document.createElement('meta');
    t.name = "viewport";
    t.content = "initial-scale=1.0, maximum-scale=1.0, user-scalable=no, height=device-height, width=device-width, viewport-fit=cover";
    document.getElementsByTagName('head')[0].appendChild(t);
    $(document).on('blur', 'input, textarea', function() {
      setTimeout(function(){ window.scrollTo(document.body.scrollLeft, document.body.scrollTop); }, 0);
    });
  );
  
  doJavaScript( fix_ios_js );
  
  
  
  WApplication::instance()->useStyleSheet( "assets/css/OwletWebApp.css" );
  
  root()->addStyleClass( "root" );
  
  m_layout = root()->setLayout( make_unique<WGridLayout>() );
  
  
  int row = 0;
  auto header = m_layout->addWidget( std::make_unique<WContainerWidget>(), row, 0, 1, 2 );
  
  auto settings = header->addWidget( std::make_unique<WImage>("assets/images/noun_Settings_3144389.svg", "Settings") );
  settings->addStyleClass( "SettingsIcon" );
  settings->clicked().connect( [this](){
    if( !m_settings )
      m_settings = root()->addWidget( make_unique<SettingsDialog>() );
    m_settings->finished().connect( [=](){
      if( m_settings )
        m_settings->removeFromParent();
      m_settings = nullptr;
    });
  });
  
  
  m_status = header->addWidget( std::make_unique<WText>("&nbsp;") );
  m_status->addStyleClass( "StatusTxt" );
  
  
  ++row;
  m_oxygen_disp = m_layout->addWidget( std::make_unique<WContainerWidget>(), row, 0, AlignmentFlag::Middle | AlignmentFlag::Center );
  m_oxygen_disp->addStyleClass( "CurrentOxygen" );
  
  auto icon = m_oxygen_disp->addWidget( make_unique<WImage>( WLink("assets/images/noun_Lungs_2911021.svg") ) );
  icon->setHeight( WLength(25,LengthUnit::Pixel) );
  icon->addStyleClass( "InfoIcon" );
  //auto label = m_oxygen_disp->addWidget( make_unique<WLabel>("Current Oxygen: ") );
  m_current_oxygen = m_oxygen_disp->addWidget( make_unique<WText>("--") );
  m_current_oxygen->addStyleClass( "CurrentValueTxt" );
  auto postfix = m_oxygen_disp->addWidget( make_unique<WText>("%") );
  postfix->addStyleClass( "CurrentValueUnits" );
  
  
  m_heartrate_disp = m_layout->addWidget( std::make_unique<WContainerWidget>(), row, 1, AlignmentFlag::Middle | AlignmentFlag::Center );
  m_heartrate_disp->addStyleClass( "CurrentHeart" );
  
  icon = m_heartrate_disp->addWidget( make_unique<WImage>( WLink("assets/images/noun_Heart_3432419.svg") ) );
  icon->setHeight( WLength(25,LengthUnit::Pixel) );
  icon->addStyleClass( "InfoIcon" );
  m_current_heartrate = m_heartrate_disp->addWidget( make_unique<WText>("--") );
  m_current_heartrate->addStyleClass( "CurrentValueTxt" );
  postfix = m_heartrate_disp->addWidget( make_unique<WText>("&nbsp;BPM") );
  postfix->addStyleClass( "CurrentValueUnits" );

  
  ++row;
  m_chart = m_layout->addWidget( std::make_unique<OwletChart>(true, false), row, 0, 1, 2 );
  
  // \TODO: set height based off actual screen size
  //m_chart->setMaximumSize( WLength::Auto, WLength(400,Wt::LengthUnit::Pixel) );
  
  m_layout->setRowStretch( row, 1 );
  //m_layout->setColumnStretch( 0, 1 );
  
  
  //if( sm_oxygen_alarm.currently_alarming() )
  //  startOxygenAlarm();
  //else
    if( sm_oxygen_alarm.currently_snoozed() )
    snoozeOxygenAlarm();
  
  //if( sm_heartrate_low_alarm.currently_alarming() )
  //  startLowHeartRateAlarm();
  //else
    if( sm_heartrate_low_alarm.currently_snoozed() )
    snoozeLowHeartRateAlarm();
  
  //if( sm_heartrate_high_alarm.currently_alarming() )
  //  startHighHeartRateAlarm();
  //else
    if( sm_heartrate_high_alarm.currently_snoozed() )
    snoozeHighHeartRateAlarm();
  
  //if( sm_sock_off_alarm.currently_alarming() )
  //  startSockOffAlarm();
  //else
    if( sm_sock_off_alarm.currently_snoozed() )
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
  
  
  
  const char *heartRateUpdatedNow_js = INLINE_JAVASCRIPT( function(dt){
    
    console.log( " heartrate updated to " + dt );
      const wtroot = document.querySelector('.Wt-domRoot');
      if( wtroot )
        wtroot.dataset.heartRateUpdateTime = dt;
    }
  );
  declareJavaScriptFunction( "heartRateUpdatedNow", heartRateUpdatedNow_js );
  
  const char *oxygenUpdatedNow_js = INLINE_JAVASCRIPT( function(dt){
    console.log( "oxygenupdated to " + dt );
      const wtroot = document.querySelector('.Wt-domRoot');
      if( wtroot )
        wtroot.dataset.oxygenUpdateTime = dt;
    }
  );
  declareJavaScriptFunction( "oxygenUpdatedNow", oxygenUpdatedNow_js );
  
  
  const char *statusUpdatedNow_js = INLINE_JAVASCRIPT( function(dt,status){
      const wtroot = document.querySelector('.Wt-domRoot');
      if( wtroot )
      {
        wtroot.dataset.status = status;
        wtroot.dataset.statusUpdateTime = dt;
      }
    }
  );
  declareJavaScriptFunction( "statusUpdated", statusUpdatedNow_js );
  
  
  
  
  const string checkStatusJs = INLINE_JAVASCRIPT(
    const doupdate = function(el){
    
      const updated = function( whenup ){
        const nsec = (Date.now() - whenup) / 1000;
        if( nsec < 30 )
          return "now";
        if( nsec < 45 )
          return "30 sec. ago";
        if( nsec < 45 )
          return "1 min. ago";
        if( nsec < 105 )
          return "1.5 min. ago";
        if( nsec < 150 )
          return "2 min. ago";
        
        const nminute = Math.floor( nsec / 60 );
        return nminute + " min. ago";
      };
    
      const wtroot = document.querySelector('.Wt-domRoot');
    
      let txt = "";
      if( wtroot && wtroot.dataset.status )
        txt += wtroot.dataset.status + ". ";
      if( wtroot && wtroot.dataset.oxygenUpdateTime )
        txt += "O2 " + updated(wtroot.dataset.oxygenUpdateTime) + ".  ";
      if( wtroot && wtroot.dataset.heartRateUpdateTime )
        txt += "HR " + updated(wtroot.dataset.heartRateUpdateTime) + ". ";
      if( wtroot && wtroot.dataset.statusUpdateTime )
        txt += "Status " + updated(wtroot.dataset.statusUpdateTime) + ".";
      
      el.innerHTML = txt;
      
      const opac = function( whenup ){
        return Math.max(0.1, 1.0 - (Date.now() - whenup) / 180000 );
      };
    
      $('.CurrentOxygen').css({ opacity: opac(wtroot.dataset.oxygenUpdateTime) });
      $('.CurrentHeart').css({ opacity: opac(wtroot.dataset.heartRateUpdateTime) });
    };
  );
  const string txtrf = m_status->jsRef();
  
  root()->doJavaScript( checkStatusJs + "setInterval( function(){ doupdate(" + txtrf + "); }, 5000);");
  
  if( latest_status )
    updateStatusToClient( *latest_status );
  
  if( last_oxygen_time.isValid() && last_oxygen )
    updateOxygenToClient( last_oxygen_time, last_oxygen );
  
  if( last_heartrate_time.isValid() && last_heartrate )
    updateHeartRateToClient( last_heartrate_time, last_heartrate );
  
  checkInitialAutoAudioPlay();
}//OwletWebApp constructor


void OwletWebApp::updateOxygenToClient( const Wt::WDateTime &last_oxygen_time, const int last_oxygen )
{
  m_current_oxygen->setText( std::to_string(last_oxygen) + "" );
  //const auto now = WDateTime::currentDateTime();
  //auto localdate = last_oxygen_time.addSecs( 60* wApp->environment().timeZoneOffset().count() );
  //const char *format = (now.date() == last_oxygen_time.date()) ? "hh:mm:ss" : "ddd hh:mm:ss";
  

  //const char *toJsDateFmt = "yyyy-MM-dd hh:mm:ss";
  //doJavaScript( javaScriptClass() + ".oxygenUpdatedNow( new Date(\"" + localdate.toString(toJsDateFmt).toUTF8() + "\") );" );
  
  doJavaScript( javaScriptClass() + ".oxygenUpdatedNow(" + to_string(1000*last_oxygen_time.toTime_t()) + ");" );
}//void updateOxygenToClient( const Wt::WDateTime &utc, const int value, const int moving )


void OwletWebApp::updateHeartRateToClient( const Wt::WDateTime &last_heartrate_time, const int last_heart )
{
  m_current_heartrate->setText( std::to_string(last_heart) + "" );
  //const auto now = WDateTime::currentDateTime();
  //auto localdate = last_heartrate_time.addSecs( 60* wApp->environment().timeZoneOffset().count() );
  //const char *format = (now.date() == last_heartrate_time.date()) ? "hh:mm:ss" : "ddd hh:mm:ss";

  //const char *toJsDateFmt = "yyyy-MM-dd hh:mm:ss";
  //doJavaScript( javaScriptClass() + ".heartRateUpdatedNow( new Date(\"" + localdate.toString(toJsDateFmt).toUTF8() + "\") );" );
  
  doJavaScript( javaScriptClass() + ".heartRateUpdatedNow(" + to_string(1000*last_heartrate_time.toTime_t()) + ");" );
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
  {
    txt += "<b>Sock Off</b>";
    if( !m_status->hasStyleClass("Alarming") )
      m_status->addStyleClass( "Alarming" );
  }else if( m_status->hasStyleClass("Alarming") )
  {
    m_status->removeStyleClass( "Alarming" );
  }
  
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
    
  const WDateTime datetime = utcDateTimeFromStr(status.utc_date);

  doJavaScript( javaScriptClass() + ".statusUpdated(" + to_string(1000*datetime.toTime_t()) + ",'" + txt + "');" );
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
  
  //If we start auto-playing, but the user hasnt done the manually clicking of play to allow
  //  auto play (like if we load the page while it is already alarming!), then we will get an
  //  uncaught exception or something, and app will fail and we cause a refresh, making it
  //  so we cant dismiss the alarm.  So we will manulaly play the audio with JS using a promise.
  auto audio = m_oxygen_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->setOptions( /* PlayerOption::Autoplay | */ PlayerOption::Loop );
  audio->addSource( "assets/audio/CheckOnAriMommy.mp3" );
  audio->setPreloadMode( MediaPreloadMode::Auto );

  /// \TODO: put this JS all in a function, instead of repeating it for every alarm call!
  string js =
  "let playPromise = " + audio->jsRef() + ".play();"
  "if (playPromise !== undefined) {"
  "  playPromise.then(() => {"
  "    console.log( 'Successfully starte O2 alarm playing' );"
  // + m_auto_play_test->createCall( {"true"} ) +
  "  }).catch(error => {"
  "    console.log( 'Failed auto-play of O2 alarm with error: ' + error.name );"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "  });"
  "}else{"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "}";
  audio->doJavaScript( js );
  
  
  
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

  //See notes from O2 alarm for why we play using JS
  auto audio = m_low_heartrate_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->addSource("assets/audio/CheckOnAriMommy.mp3");
  audio->setOptions( /* PlayerOption::Autoplay |*/ PlayerOption::Loop ); //PlayerOption::Controls
  audio->setPreloadMode( MediaPreloadMode::Auto );
  //audio->play();
  
  string js =
  "let playPromise = " + audio->jsRef() + ".play();"
  "if (playPromise !== undefined) {"
  "  playPromise.then(() => {"
  "    console.log( 'Successfully starte O2 alarm playing' );"
  // + m_auto_play_test->createCall( {"true"} ) +
  "  }).catch(error => {"
  "    console.log( 'Failed auto-play of O2 alarm with error: ' + error.name );"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "  });"
  "}else{"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "}";
  audio->doJavaScript( js );
  
  
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

  // See notes for O2 heartrate of why we play using JS
  auto audio = m_high_heartrate_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->addSource("assets/audio/CheckOnAriMommy.mp3");
  audio->setOptions( /* PlayerOption::Autoplay |*/ PlayerOption::Loop ); //PlayerOption::Controls
  audio->setPreloadMode( MediaPreloadMode::Auto );
  //audio->play();
  
  string js =
  "let playPromise = " + audio->jsRef() + ".play();"
  "if (playPromise !== undefined) {"
  "  playPromise.then(() => {"
  "    console.log( 'Successfully starte O2 alarm playing' );"
  // + m_auto_play_test->createCall( {"true"} ) +
  "  }).catch(error => {"
  "    console.log( 'Failed auto-play of O2 alarm with error: ' + error.name );"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "  });"
  "}else{"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "}";
  audio->doJavaScript( js );
  
  
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

  // See notes for O2 alarm for why we play using JS
  auto audio = m_sock_off_mb->contents()->addWidget( make_unique<WAudio>() );
  audio->addSource("assets/audio/CheckOnAriMommy.mp3");
  audio->setOptions( /* PlayerOption::Autoplay | */ PlayerOption::Loop ); //PlayerOption::Controls
  audio->setPreloadMode( MediaPreloadMode::Auto );
  //audio->play();
  
  string js =
  "let playPromise = " + audio->jsRef() + ".play();"
  "if (playPromise !== undefined) {"
  "  playPromise.then(() => {"
  "    console.log( 'Successfully starte O2 alarm playing' );"
  // + m_auto_play_test->createCall( {"true"} ) +
  "  }).catch(error => {"
  "    console.log( 'Failed auto-play of O2 alarm with error: ' + error.name );"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "  });"
  "}else{"
  //  + m_auto_play_test->createCall( {"false"} ) +
  "}";
  audio->doJavaScript( js );
  
  
  
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


void OwletWebApp::checkInitialAutoAudioPlay()
{
  // For some browsers apparently playing audio from an iframe will allow auto-playing of audio
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
  
  
  // Lets do a check if audio can be auto-played, and if not have the user manually play some so the
  //  the audio can be auto-played.
  /// Well add the audio to a random element since we wont be showing controls anyway
  auto audio = m_oxygen_disp->addWidget( make_unique<WAudio>() );
  audio->addSource( "assets/audio/silence.mp3" );
  audio->setPreloadMode( MediaPreloadMode::Auto );
  audio->pause(); //force call to loadJavaScript();
  

  m_auto_play_test = std::make_unique<Wt::JSignal<bool>>( this, "canplayaudio" );
  
  string js =
  "let startPlayPromise = " + audio->jsRef() + ".play();"
  "if (startPlayPromise !== undefined) {"
  "  startPlayPromise.then(() => {"
  "    console.log( 'Successfully played' );"
    + m_auto_play_test->createCall( {"true"} ) +
  "  }).catch(error => {"
  "    console.log( 'Failed auto-play with error: ' + error.name );"
    + m_auto_play_test->createCall( {"false"} ) +
  "  });"
  "}else{"
    + m_auto_play_test->createCall( {"false"} ) +
  "}";
  
  
  m_auto_play_test->connect( [this,audio]( const bool played ){
    m_auto_play_test.reset();
    m_oxygen_disp->removeWidget( audio );
    if( played )
      doInitialAlarming();
    else
      audioPlayPopup();
  } );
  
  audio->doJavaScript( js );
}//void checkInitialAutoAudioPlay()


void OwletWebApp::doInitialAlarming()
{
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
}//void doInitialAlarming()


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
  
  audio->ended().connect( [=](){
    message->accept();
    message->removeFromParent();
    
    // \TODO: test that audio can actually autoplay now
    
    doInitialAlarming();
  });
  audio->setInline( false );
  audio->setHeight( 45 );
  
  auto txt = message->contents()->addWidget( make_unique<WText>( "This is necassary to allow the"
                                                                " browser to auto-play audio." ) );
  txt->setInline( false );
  
  WPushButton *btn = message->addButton( "Skip", StandardButton::Ok );
  btn->clicked().connect( [this,message](){
    message->accept();
    message->removeFromParent();
    doInitialAlarming();
  } );
  
  message->setModal( true );
  message->show();
}//void audioPlayPopup();


bool OwletWebApp::isShowingFiveMinuteAvrg()
{
  return m_chart->isShowingFiveMinuteAvrg();
}//bool isShowingFiveMinuteAvrg()


void OwletWebApp::showFiveMinuteAvrgData()
{
  m_chart->showFiveMinuteAvrgData();
}//void showFiveMinuteAvrgData()


void OwletWebApp::showIndividualPointsData()
{
  m_chart->showIndividualPointsData();
}//void showIndividualPointsData()


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


void OwletWebApp::alarm_thresholds_updated()
{
  auto server = WServer::instance();
  assert( server );
  
  int o2level = 0, hrlower = 0, hrupper = 0;

  {
    std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_oxygen_alarm.m_mutex );
    o2level = OwletWebApp::sm_oxygen_alarm.m_threshold;
  }
  
  {
    std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
    hrlower = OwletWebApp::sm_heartrate_low_alarm.m_threshold;
  }
  
  {
    std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_high_alarm.m_mutex );
    hrupper = OwletWebApp::sm_heartrate_high_alarm.m_threshold;
  }
  
  
  server->postAll( [=](){
    auto app = dynamic_cast<OwletWebApp *>( WApplication::instance() );
    if( !app )
      return;
    
    app->m_chart->oxygenAlarmLevelUpdated( o2level );
    app->m_chart->heartRateAlarmLevelUpdated( hrlower, hrupper );
    
    app->triggerUpdate();
  });
}//void alarm_thresholds_updated()


void OwletWebApp::set_error( string msg )
{
  
}




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
      if( app->m_settings )
        app->m_settings->m_settings->setThresholdsFromIniToGui();
      app->triggerUpdate();
    }else
    {
      cerr << "Got dead instance in updateSettings?" << endl;
    }
  });
  
  server->ioService().post( [](){ save_ini(); } );
}//void set_config_value(...)


