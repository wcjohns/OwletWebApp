#include <tuple>
#include <vector>
#include <string>
#include <limits>

#include <Wt/WComboBox.h>
#include <Wt/WCheckBox.h>
#include <Wt/WDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WGridLayout.h>
#include <Wt/WEnvironment.h>
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/Chart/WCartesianChart.h>
#include <Wt/Chart/WAxisSliderWidget.h>
#include <Wt/Chart/WAbstractChartModel.h>


#include "Database.h"
#include "OwletChart.h"


using namespace std;
using namespace Wt;

const char * const TO_CLIENT_DATETIME_FORMAT = "yyyy-MM-ddThh:mm:ss";

std::string to_client_str( const Wt::WDateTime &utc )
{
  //const int nminutes_offsets = wApp->environment().timeZoneOffset().count();  //-420
  //cout << "utc.date().day()=" << utc.date().day() << ", time=" << utc.time().hour() << ":" << utc.time().minute() << ":" << utc.time().second() << endl;
  //cout << "nminutes_offsets=" << nminutes_offsets << endl;
  //const auto localtime = utc.addSecs( 60 * nminutes_offsets );
  //return localtime.toString(TO_CLIENT_DATETIME_FORMAT,false).toUTF8();
  return utc.toString(TO_CLIENT_DATETIME_FORMAT,false).toUTF8();
}

/** Model for displaying the chart from; will use the global variables to display from (so all app sessions can share the same meory),
 however will track the current ending index so we can properly emit updated data when added or removed.
 
 A downside of this approach is we have to aquire the g_data_mutex mutex each time we get data, but it looks like this isnt that often
 when #WCartesianChart::setOnDemandLoadingEnabled is set to true, so we wont worry about it right now.
 
 Row: index of the DateTime for a reading
 Column: { 0: DateTime, 1: Oxygen, 2: HeartRate, 3: Moving: 4: Connected, 5: Battery  }
 */
class OwletChartModel : public Wt::Chart::WAbstractChartModel
{
  int m_time_offset_seconds;
  size_t m_end_index;
  
public:
  OwletChartModel()
    : WAbstractChartModel(),
    m_time_offset_seconds( 0 ),
    m_end_index( 0 )
  {
    m_time_offset_seconds = 60*WApplication::instance()->environment().timeZoneOffset().count();
    
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      m_end_index = g_readings.size();
    }//end lock on g_data_mutex
    
    cerr << "m_end_index=" << m_end_index << endl;
  }//OwletChartModel()
  
  void addedData( const size_t num_readings_before, const size_t num_readings_after )
  {
    assert( num_readings_before < num_readings_after );
    
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      assert( num_readings_after <= g_readings.size() );
    }//end lock on g_data_mutex
    
    //const size_t previous_end = m_end_index;
    m_end_index = num_readings_after;
    
    changed().emit();
  }//void addedData(...)
  
  double maxDate() const
  {
    std::lock_guard<mutex> data_lock( g_data_mutex );
    if( !g_readings.size() )
      return 0.0;
    
    const auto &reading = g_readings.back();
    return m_time_offset_seconds + reading.utc_time.toTime_t();
  }
  
  
  virtual double data( int row, int column ) const
  {
#define NaN_VALUE numeric_limits<double>::quiet_NaN()
    
    if( column < 0 || column > 5 )
      return NaN_VALUE;
    
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      if( row < 0 || row >= static_cast<int>(m_end_index) )
        return NaN_VALUE;
      
      assert( row < static_cast<int>( g_readings.size() ) );
      
      const auto &reading = g_readings[row];
      
      switch( column )
      {
          // { 0: DateTime, 1: Oxygen, 2: HeartRate, 3: Moving: 4: Connected, 5: Battery  }
        case 0:
          //cout << "Data(" << row << "," << column << ") --> " << reading.utc_time.toTime_t()
          //    << " (" << reading.utc_time.toString() << ")" << endl;
          return m_time_offset_seconds + reading.utc_time.toTime_t();
        case 1:
          //cout << "Data(" << row << "," << column << ") --> " << static_cast<double>(reading.oxygen) << endl;
          //return (reading.oxygen > 0) ? static_cast<double>(reading.oxygen) : NaN_VALUE;
          
          if( reading.oxygen > 0 )
          {
            return reading.oxygen;
          }else
          {
            for( int r = row; r > 0; --r )
              if( g_readings[r].oxygen > 0 )
                return g_readings[r].oxygen;
            return NaN_VALUE;
          }
        case 2:
          return (reading.heartrate > 0) ? static_cast<double>(reading.heartrate) : NaN_VALUE;
        case 3:
          return (reading.movement > 0) ? static_cast<double>(reading.movement) : NaN_VALUE;
        case 4:
          return (reading.sock_connection > 0) ? static_cast<double>(reading.sock_connection) : NaN_VALUE;
        case 5:
          return (reading.battery > 0) ? static_cast<double>(reading.battery) : NaN_VALUE;
      }//switch( column )
    }//end lock on g_data_mutex
    
    assert(0);
    return NaN_VALUE;
  }
  
  virtual WString headerData( int column ) const
  {
    switch( column )
    {
        // { 0: DateTime, 1: Oxygen, 2: HeartRate, 3: Moving: 4: Connected, 5: Battery  }
      case 0: return "DateTime";
      case 1: return "Oxygen";
      case 2: return "Heart Rate";
      case 3: return "Movement";
      case 4: return "Sock Connected";
      case 5: return "Battery Level";
    }//switch( column )
    
    return WString();
  }//headerData
  
  virtual WString toolTip( int row, int column ) const
  {
    return WString();
  }
  
  virtual int columnCount() const
  {
    return 5;
  }
   
  virtual int rowCount() const
  {
    return static_cast<int>( m_end_index );
  }
  
  //Additional functions we could use to customize the chart a little bit.
  //virtual const WColor *markerPenColor (int row, int column) const
  //virtual const WColor *markerBrushColor (int row, int column) const
  //virtual const MarkerType *markerType (int row, int column) const
  //virtual const WColor *barPenColor (int row, int column) const
  //virtual const double *markerScaleFactor (int row, int column) const
};//class OwletChartModel


enum DurationIndex
{
  QuarterHour,
  HalfHour,
  OneHour,
  ThreeHours,
  SixHours,
  TwelveHours,
  OneDay,
  TwoDays,
  FourDays,
  OneWeek,
  TwoWeeks,
  FourWeeks,
  CustomDuration,
  NumDurationIndexs
};//enum DurationIndex


OwletChart::OwletChart( const bool oxygen, const bool heartrate )
: m_chart( nullptr ), m_model( nullptr )/*,  m_oxygen( oxygen ), m_heartrate( heartrate ) */
{
  addStyleClass( "OwletChart" );
  
  m_last_oxygen = 100;
  m_last_heartrate = 100;
  
  WGridLayout *layout = setLayout( make_unique<WGridLayout>() );
  
  m_chart = layout->addWidget( std::make_unique<Chart::WCartesianChart>(), 0, 0, 1, 3 );
  m_model = make_shared<OwletChartModel>();
  m_chart->setModel( m_model );
  m_chart->setXSeriesColumn(0);
  m_chart->setType(Chart::ChartType::Scatter);
  m_chart->setOnDemandLoadingEnabled( true );
  
  auto &xaxis = m_chart->axis(Wt::Chart::Axis::X);
  xaxis.setScale(Chart::AxisScale::Date);
  xaxis.setMinimumZoomRange(  15*60 );
  xaxis.setMaximumZoomRange( 28*24*3600 );
  xaxis.setScale( Chart::AxisScale::DateTime );
  
  
  auto s = cpp14::make_unique<Chart::WDataSeries>(1, Chart::SeriesType::Line, Chart::Axis::Y1 );
  auto s_ = s.get();
  //s->setShadow(WShadow(3, 3, WColor(0, 0, 0, 127), 3));
  m_chart->addSeries( std::move(s) );

  WPen linepen( StandardColor::DarkBlue );
  s_->setPen( linepen );
  m_chart->setBackground(WColor(220, 220, 220));
  
  // Enable pan and zoom
  m_chart->setPanEnabled(true);
  m_chart->setZoomEnabled(true);
  //m_chart->setRubberBandEffectEnabled( true );
  m_chart->setCrosshairEnabled( false );
  //m_chart->setFollowCurve( 1 );
  
  
  std::map<WFlags<KeyboardModifier>, Chart::InteractiveAction> wheelActions;
  wheelActions[KeyboardModifier::None] = Chart::InteractiveAction::PanX;
  wheelActions[KeyboardModifier::Control] = Chart::InteractiveAction::ZoomX;
  m_chart->setWheelActions( wheelActions );

  //m_chart->setMargin(WLength::Auto, Side::Left | Side::Right); // Center horizontally

  // Add a WAxisSliderWidget for the chart using the data series for column 2
  //auto sliderWidget = layout->addWidget( make_unique<Chart::WAxisSliderWidget>(s_), 1, 0, 1, 3 );
  //sliderWidget->setHeight( 80 );
  //sliderWidget->setSelectionAreaPadding(40, Side::Left | Side::Right);
  //sliderWidget->setMargin(WLength::Auto, Side::Left | Side::Right); // Center horizontally
  
  
  double end_time = WDateTime::currentDateTime().toTime_t()
                    + 60*wApp->environment().timeZoneOffset().count();
  double start_time = end_time - 12*60*60;
  
  const int nrows = m_model->rowCount();
  if( nrows )
  {
    end_time = m_model->data(nrows-1,0);
    start_time = end_time - 12*3600;
    xaxis.setZoomRange( start_time, end_time );
    xaxis.setMaximum( end_time + 3600 );
  }//if( nrows )
  
  m_chart->axis(Wt::Chart::Axis::Y1).setRange( 91, 101 );
  xaxis.zoomRangeChanged().connect( this, &OwletChart::zoomRangeChangeCallback );
  
  m_previous_range = layout->addWidget( make_unique<WPushButton>("Previous"), 2, 0, AlignmentFlag::Right );
  m_duration_select = layout->addWidget( make_unique<WComboBox>(), 2, 1, AlignmentFlag::Center );
  m_next_range = layout->addWidget( make_unique<WPushButton>("Next"), 2, 2, AlignmentFlag::Left );
  
  for( DurationIndex index = DurationIndex(0);
      index < DurationIndex::NumDurationIndexs;
      index = DurationIndex(index+1) )
  {
    const char *txt = "";
    switch( index )
    {
      case DurationIndex::QuarterHour:    txt = "15 minutes"; break;
      case DurationIndex::HalfHour:       txt = "30 minutes"; break;
      case DurationIndex::OneHour:        txt = "1 hour"; break;
      case DurationIndex::ThreeHours:     txt = "3 hours"; break;
      case DurationIndex::SixHours:       txt = "6 hours"; break;
      case DurationIndex::TwelveHours:    txt = "12 hours"; break;
      case DurationIndex::OneDay:         txt = "24 hours"; break;
      case DurationIndex::TwoDays:        txt = "2 days"; break;
      case DurationIndex::FourDays:       txt = "4 days"; break;
      case DurationIndex::OneWeek:        txt = "1 week"; break;
      case DurationIndex::TwoWeeks:       txt = "2 weeks"; break;
      case DurationIndex::FourWeeks:      txt = "4 weeks"; break;
      case DurationIndex::CustomDuration: txt = "Custom Duration"; break;
      case DurationIndex::NumDurationIndexs:
        assert( 0 );
        break;
    }//switch( index )
    
    m_duration_select->addItem( txt );
  }//for( loop over duration )
  
  m_duration_select->setCurrentIndex( DurationIndex::TwelveHours );
  m_duration_select->activated().connect( this, &OwletChart::changeDurationCallback );

  Wt::WCheckBox *m_oxygen;
  Wt::WCheckBox *m_heartrate;
  
  layout->setRowStretch( 0, 1 );
  
  zoomRangeChangeCallback( start_time, end_time );
}//OwletChart()


void OwletChart::previousTimeRangeCallback()
{
  const double prev_start_time = m_chart->axis(Chart::Axis::X).minimum();
  const double prev_end_time = m_chart->axis(Chart::Axis::X).maximum();
  double duration = prev_end_time - prev_start_time;
  
  double data_start_time = prev_start_time;
  
  const int nrows = m_model->rowCount();
  if( nrows )
    data_start_time = m_model->data(0,0);
  
  if( (prev_start_time - duration) < data_start_time )
    duration = prev_start_time - data_start_time;
  
  m_next_range->enable();
}//void previousTimeRangeCallback()


void OwletChart::nextTimeRangeCallback()
{
  
}//void nextTimeRangeCallback()


void OwletChart::changeDurationCallback()
{
  const auto current = static_cast<DurationIndex>( m_duration_select->currentIndex() );
  double end_time = m_chart->axis(Chart::Axis::X).maximum();
  
  double data_end_time = end_time;
  const int nrows = m_model->rowCount();
  if( nrows )
    data_end_time = m_model->data(nrows-1,0);
  if( end_time > data_end_time )
    end_time = data_end_time;
  double start_time = end_time;
  
  switch (current )
  {
    case QuarterHour: start_time -= 15*60;       break;
    case HalfHour:    start_time -= 30*60;       break;
    case OneHour:     start_time -= 60*60;       break;
    case ThreeHours:  start_time -= 3*60*60;     break;
    case SixHours:    start_time -= 6*60*60;     break;
    case TwelveHours: start_time -= 12*60*60;    break;
    case OneDay:      start_time -= 24*60*60;    break;
    case TwoDays:     start_time -= 2*24*60*60;  break;
    case FourDays:    start_time -= 4*24*60*60;  break;
    case OneWeek:     start_time -= 7*24*60*60;  break;
    case TwoWeeks:    start_time -= 14*24*60*60; break;
    case FourWeeks:   start_time -= 28*24*60*60; break;
    case CustomDuration:
      start_time = m_chart->axis(Chart::Axis::X).minimum();
      if( start_time > data_end_time )
          start_time = data_end_time - 12*60*60;
      break;
    case NumDurationIndexs:
      assert( 0 );
      break;
  }//switch (current )
  
  m_chart->axis(Chart::Axis::X).setZoomRange( start_time, end_time );
  zoomRangeChangeCallback( start_time, end_time );
}//void changeDurationCallback()


void OwletChart::zoomRangeChangeCallback( double x1, double x2 )
{
  const double epsilon = 15;
  const double nseconds = std::round(x2 - x1);
  
  DurationIndex index = DurationIndex::CustomDuration;
  if( fabs(nseconds - 15*60) < epsilon )
    index = DurationIndex::QuarterHour;
  else if( fabs(nseconds - 30*60) < epsilon )
    index = DurationIndex::HalfHour;
  else if( fabs(nseconds - 60*60) < epsilon )
    index = DurationIndex::OneHour;
  else if( fabs(nseconds - 3*60*60) < epsilon )
    index = DurationIndex::ThreeHours;
  else if( fabs(nseconds - 6*60*60) < epsilon )
    index = DurationIndex::SixHours;
  else if( fabs(nseconds - 12*60*60) < epsilon )
    index = DurationIndex::TwelveHours;
  else if( fabs(nseconds - 24*60*60) < epsilon )
    index = DurationIndex::OneDay;
  else if( fabs(nseconds - 2*24*60*60) < epsilon )
    index = DurationIndex::TwoDays;
  else if( fabs(nseconds - 4*24*60*60) < epsilon )
    index = DurationIndex::FourDays;
  else if( fabs(nseconds - 7*24*60*60) < epsilon )
    index = DurationIndex::OneWeek;
  else if( fabs(nseconds - 14*24*60*60) < epsilon )
    index = DurationIndex::TwoWeeks;
  else if( fabs(nseconds - 28*24*60*60) < epsilon )
    index = DurationIndex::FourWeeks;
  
  const auto current = static_cast<DurationIndex>( m_duration_select->currentIndex() );
  if( current != index )
    m_duration_select->setCurrentIndex( index );
  
  //Update the the enabled status of the previous/next buttons
  const int nrows = m_model->rowCount();
  if( nrows )
  {
    const double data_begin_time = m_model->data(0,0);
    m_previous_range->setEnabled( (x1 >= data_begin_time) );
    
    const double data_end_time = m_model->data(nrows-1,0);
    m_next_range->setEnabled( (x2 < data_end_time) );
  }else
  {
    m_previous_range->disable();
    m_next_range->disable();
  }
  
  //Now set the x-axis labels
  auto &xaxis = m_chart->axis(Wt::Chart::Axis::X);
  double label_interval = nseconds / 10;
  const int nminutes = nseconds / 60;
  auto lowerdt = WDateTime::fromTime_t( static_cast<std::time_t>(x1) );
  lowerdt.addSecs( 60 - lowerdt.time().second() ); //round up to the next minute
  
  if( nminutes < 20 )
  {
    //Find first even minute
    if( (lowerdt.time().minute() % 2) != 0 )
      lowerdt.addSecs( 60 );
    label_interval = 120;
  }else if( nminutes < 1*60 )
  {
    //Find first multiple of 5
    if( (lowerdt.time().minute() % 5) != 0 )
      lowerdt.addSecs( 300 - 60*(lowerdt.time().minute() % 5) );
    label_interval = 300;
  }else if( nminutes < 3*60 )
  {
    //Find first multiple of 15
    if( (lowerdt.time().minute() % 15) != 0 )
      lowerdt.addSecs( 15*60 - 60*(lowerdt.time().minute() % 15) );
    label_interval = 20*60;
  }else if( nminutes < 4*60 )
  {
    //Find first multiple of 20
    if( (lowerdt.time().minute() % 20) != 0 )
      lowerdt.addSecs( 20*60 - 60*(lowerdt.time().minute() % 20) );
    label_interval = 30*60;
  }else if( nminutes < 6*60 )
  {
    //Find first multiple of 30
    if( (lowerdt.time().minute() % 30) != 0 )
      lowerdt.addSecs( 30*60 - 60*(lowerdt.time().minute() % 30) );
    label_interval = 30*60;
  }else if( nminutes < 3*24*60 )
  {
    //Find first multiple of an hour
    if( (lowerdt.time().minute() % 60) != 0 )
      lowerdt.addSecs( 60*60 - 60*(lowerdt.time().minute() % 60) );
    label_interval = 6*60*60;
  }else
  {
    //Find first day
    if( (lowerdt.time().minute() % 60) != 0 )
      lowerdt.addSecs( 60*60 - 60*(lowerdt.time().minute() % 60) );
    if( lowerdt.time().hour() != 0 )
      lowerdt.addSecs( 24*60*60 - 60*60*lowerdt.time().hour() - 60*lowerdt.time().minute() );
    label_interval = 60 * nminutes / 10; // \TODO: fix this
    label_interval = std::round(label_interval / 24 / 3600);
    if( label_interval < 1 )
      label_interval = 1;
    label_interval *= 24 * 2600;
  }//if( time range is this size ) / else ...

  xaxis.setLabelBasePoint( static_cast<double>(lowerdt.toTime_t()) );
  xaxis.setLabelInterval( label_interval );
  
  //If upper limit is today, just display time, else display date.
  //m_chart->axis(Wt::Chart::Axis::X).setLabelFormat(<#const WString &format#>);
}//void zoomRangeChangeCallback( double x1, double x2 )


void OwletChart::addedData( const size_t num_readings_before, const size_t num_readings_after )
{
  auto &xaxis = m_chart->axis(Wt::Chart::Axis::X);
  
  const double prev_max_date = m_model->maxDate();
  const double prev_max_x = xaxis.maximum();
  const double prev_min_x = xaxis.minimum();
  
  m_model->addedData( num_readings_before, num_readings_after );
  
  const double end_time = m_model->maxDate();
  xaxis.setMaximum( end_time + 3600 );
  
  if( prev_max_x >= (prev_max_date - 60) )
  {
    const double new_x_end = std::max( end_time, end_time + (prev_max_x - prev_max_date) );
    const double new_x_min = new_x_end - (prev_max_x - prev_min_x);
    xaxis.setZoomRange( new_x_min, new_x_end );
    
    zoomRangeChangeCallback( new_x_min, new_x_end );
  }//if( user could see most recent data )
}//void addedData( const size_t num_readings_before, const size_t num_readings_after )


void OwletChart::updateData( const vector<tuple<Wt::WDateTime,int,int,int>> &data )
{
  
}//void updateData( const vector<tuple<string,int,int,int>> &data )


void OwletChart::setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end )
{
  //m_chart->axis(Wt::Chart::Axis::X).setRange( asNumber(start), asNumber(end) );
  m_chart->axis(Wt::Chart::Axis::X).setZoomRange( asNumber(start), asNumber(end) );
  //m_chart->axis(Wt::Chart::Axis::X).setLabelBasePoint( ... );
  //m_chart->axis(Wt::Chart::Axis::X).setMaximum(end_time);
}//void setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end );


OwletChart::~OwletChart()
{
}

