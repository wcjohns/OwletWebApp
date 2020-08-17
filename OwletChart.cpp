#include <tuple>
#include <cmath>
#include <vector>
#include <string>
#include <limits>

#include <Wt/WImage.h>
#include <Wt/WPainter.h>
#include <Wt/WComboBox.h>
#include <Wt/WDateTime.h>
#include <Wt/WGridLayout.h>
#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>
#include <Wt/WContainerWidget.h>
#include <Wt/Chart/WCartesianChart.h>
#include <Wt/Chart/WAbstractChartModel.h>


#include "Database.h"
#include "OwletChart.h"
#include "OwletWebApp.h"


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


#define NaN_VALUE numeric_limits<double>::quiet_NaN()


class OwletChartModel : public Wt::Chart::WAbstractChartModel
{
protected:
  const int m_time_offset_seconds;
  
public:
  OwletChartModel()
  : WAbstractChartModel(),
  m_time_offset_seconds( 60*WApplication::instance()->environment().timeZoneOffset().count() )
  {
  }
  
  virtual void addedData( const size_t num_readings_before, const size_t num_readings_after ) = 0;
  virtual double minDate() const = 0;
  virtual double maxDate() const = 0;
  virtual void getAxisRange( const int column, const double x1, const double x2,
                                  double &ymin, double &ymax ) const = 0;
  
  
  virtual WString displayData( int row, int column ) const
  {
    //Doesnt seem to ever be called when only plotting the data
    const double dataflt = std::round( data(row,column) );
    
    switch ( column )
    {
      case 0:
      {
        const time_t datetime = static_cast<time_t>( round(dataflt) ); //dateflt - m_time_offset_seconds
        const WDateTime dateobj = WDateTime::fromTime_t( datetime );
        return WString::fromUTF8( to_client_str(dateobj) );
      }
        
      case 1: return WString::fromUTF8( std::to_string( static_cast<int>(dataflt)) );
      case 2: return WString::fromUTF8( std::to_string( static_cast<int>(dataflt)) );
    }
    
    return WAbstractChartModel::displayData( row, column );
  }//displayData(...)
  
  
  virtual WFlags<ItemFlag> flags( int row, int column ) const
  {
    if( column != 1 && column != 2)
      return WFlags<ItemFlag>{};
    
    return ItemFlag::XHTMLText | ItemFlag::DeferredToolTip;
    //return ItemFlag::XHTMLText;
    //return ItemFlag::DeferredToolTip;
  }
  
  virtual WString toolTip( int row, int column ) const
  {
    if( column != 1 && column != 2 )
      return WString();
    
    const double dateflt = data( row, 0 );
    const double value = data( row, column );
    
    const int dataint = static_cast<int>( round(value) );
    const time_t datetime = static_cast<time_t>( round(dateflt) ); //dateflt - m_time_offset_seconds
    const WDateTime dateobj = WDateTime::fromTime_t( datetime );
    const char *suffix = (column==1) ? "% O<sub>2</sub>" : " BPM";
    
    WString answer( "{1}{2}, {3}");
    answer.arg( dataint ).arg( suffix ).arg( to_client_str(dateobj) );
    cout << answer << endl;
    return answer;
  }//toolTip(...)
};//class OwletChartModel;


/** Model for displaying the chart from; will use the global variables to display from (so all app sessions can share the same meory),
 however will track the current ending index so we can properly emit updated data when added or removed.
 
 A downside of this approach is we have to aquire the g_data_mutex mutex each time we get data, but it looks like this isnt that often
 when #WCartesianChart::setOnDemandLoadingEnabled is set to true, so we wont worry about it right now.
 
 Row: index of the DateTime for a reading
 Column: { 0: DateTime, 1: Oxygen, 2: HeartRate, 3: Moving: 4: Connected, 5: Battery  }
 */
class OwletDataPointModel : public OwletChartModel
{
  size_t m_end_index;
  
public:
  OwletDataPointModel()
    : OwletChartModel(),
    m_end_index( 0 )
  {
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      m_end_index = g_readings.size();
    }//end lock on g_data_mutex
  }//OwletDataPointModel()
  
  virtual void addedData( const size_t num_readings_before, const size_t num_readings_after )
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
  

  
  virtual double minDate() const
  {
    std::lock_guard<mutex> data_lock( g_data_mutex );
    if( !g_readings.size() )
      return 0.0;
    
    assert( m_end_index <= g_readings.size() );
    
    const auto &reading = g_readings[0];
    return m_time_offset_seconds + reading.utc_time.toTime_t();
  }
  
  
  virtual double maxDate() const
  {
    std::lock_guard<mutex> data_lock( g_data_mutex );
    if( !g_readings.size() || !m_end_index )
      return 0.0;
    
    assert( m_end_index <= g_readings.size() );
    
    const auto &reading = g_readings[m_end_index-1];
    return m_time_offset_seconds + reading.utc_time.toTime_t();
  }
  
  
  virtual double data( int row, int column ) const
  {
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
            {
              if( g_readings[r].oxygen > 0 )
              {
                const int nsec = g_readings[r].utc_time.secsTo(reading.utc_time);
                if( abs(nsec) > g_nsec_no_data )
                  return NaN_VALUE;
                return g_readings[r].oxygen;
              }
            }
            return NaN_VALUE;
          }
        case 2:
          if( reading.heartrate > 0 )
          {
            return reading.heartrate;
          }else
          {
            for( int r = row; r > 0; --r )
            {
              if( g_readings[r].heartrate > 0 )
              {
                const int nsec = g_readings[r].utc_time.secsTo(reading.utc_time);
                if( abs(nsec) > g_nsec_no_data )
                  return NaN_VALUE;
                return g_readings[r].heartrate;
              }
            }
            
            return NaN_VALUE;
          }
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
  
  
  virtual void getAxisRange( const int colum, const double x1, const double x2,
                                  double &ymin, double &ymax ) const
  {
    assert( colum == 1 || colum == 2 );
    
    if( colum == 1 )
    {
      std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_oxygen_alarm.m_mutex );
      
      ymin = OwletWebApp::sm_oxygen_alarm.m_threshold - 4;
      ymax = 101;
    }else if( colum == 2 )
    {
      {
        std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
        ymin = OwletWebApp::sm_heartrate_low_alarm.m_threshold - 5;
      }
      
      {
        std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_high_alarm.m_mutex );
        ymax = OwletWebApp::sm_heartrate_high_alarm.m_threshold + 5;
      }
    }//if( oxygen ) / else( heartrate )
    
    const auto utc_start = x1 - m_time_offset_seconds;
    const auto utc_end = x2 - m_time_offset_seconds;
    
    const WDateTime startTime = WDateTime::fromTime_t( static_cast<time_t>( round(utc_start) ) );
    const WDateTime endTime = WDateTime::fromTime_t( static_cast<time_t>( round(utc_end) ) );
    
    
    std::lock_guard<mutex> data_lock( g_data_mutex );
    
    //Find range of data points between
    auto comp = []( const OwletReading &read, const WDateTime &dt ) -> bool {
      return read.utc_time < dt;
    };
    
    auto iter = std::lower_bound( begin(g_readings), end(g_readings), startTime, comp );
    
    for( ; iter != end(g_readings) && iter->utc_time <= endTime; ++iter )
    {
      if( colum == 1 )
      {
        if( iter->oxygen > 0 )
          ymin = std::min( ymin, iter->oxygen - 1.0);
      }else if( colum == 2 )
      {
        if( iter->heartrate > 0 )
        {
          ymin = std::min( ymin, iter->heartrate - 5.0);
          ymax = std::max( ymax, iter->heartrate + 5.0);
        }
      }//if( oxygen ) / else ( heartrate )
    }//for( loop over applicable readings )
    
    
    //Lets have the lowest number always be even, so top number will be 100
    if( colum == 1 )
      ymin -= ((static_cast<int>(ymin) % 2) ? 1 : 0);
  }//getAxisRange(...)
  
  
  
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
  
  virtual int columnCount() const
  {
    return 5;
  }
   
  virtual int rowCount() const
  {
    return static_cast<int>( m_end_index );
  }
  
  //Additional functions we could use to customize the chart a little bit.
  virtual const WColor *markerPenColor( int row, int column ) const
  {
    const static WColor s_o2_color = WColor("#2E3440");
    const static WColor s_hr_color = WColor("#8FBCBB");
    
    switch( column )
    {
      case 1: return &s_o2_color;
      case 2: return &s_hr_color;
    }//switch( column )
    
    return nullptr;
  }//
  
  virtual const WColor *markerBrushColor (int row, int column) const
  {
    const static WColor s_o2_brush = WColor("#2E3440");
    const static WColor s_hr_brush = WColor("#8FBCBB");
    
    switch( column )
    {
      case 1: return &s_o2_brush;
      case 2: return &s_hr_brush;
    }//switch( column )
    
    return nullptr;
  }
  
  virtual const Chart::MarkerType *markerType( int row, int column) const
  {
    const static Chart::MarkerType s_data_point_marker = Wt::Chart::MarkerType::Circle;
    
    if( column < 0 || column > 5 )
      return nullptr;
    
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      if( row < 0 || row >= static_cast<int>(m_end_index) )
        return nullptr;
           
      assert( row < static_cast<int>( g_readings.size() ) );
           
      const auto &reading = g_readings[row];
           
      switch( column )
      {
        // { 0: DateTime, 1: Oxygen, 2: HeartRate, 3: Moving: 4: Connected, 5: Battery  }
        case 0: return nullptr;
        case 1: return ((reading.oxygen <= 0) ? nullptr : &s_data_point_marker);
        case 2: return ((reading.heartrate <= 0) ? nullptr : &s_data_point_marker);
      }//switch( column )
    }//end lock on g_data_mutex
         
    return nullptr;
  }//markerType(...)
    
  //virtual const WColor *barPenColor (int row, int column) const
  //virtual const double *markerScaleFactor (int row, int column) const
};//class OwletDataPointModel



class OwletFiveMinuteModel : public OwletChartModel
{
  static const int sm_nsec_interval = 300;
  
  Wt::WDateTime m_start_time, m_end_time;
  
public:
  OwletFiveMinuteModel()
    : OwletChartModel()
  {
    std::lock_guard<mutex> data_lock( g_data_mutex );
    if( !g_readings.empty() )
    {
      m_start_time = g_readings.front().utc_time;
      m_end_time = g_readings.back().utc_time;
    }
  }//OwletFiveMinuteModel()
  
  virtual void addedData( const size_t num_readings_before, const size_t num_readings_after )
  {
    {//begin lock on g_data_mutex
      std::lock_guard<mutex> data_lock( g_data_mutex );
      assert( num_readings_after <= g_readings.size() );
      
      m_start_time = g_readings.front().utc_time;
      m_end_time = g_readings.back().utc_time;
    }//end lock on g_data_mutex
    
    changed().emit();
  }//void addedData(...)
  

  
  virtual double minDate() const
  {
    if( !m_start_time.isValid() )
      return 0.0;
    
    return m_time_offset_seconds + m_start_time.toTime_t();
  }
  
  
  virtual double maxDate() const
  {
    if( !m_end_time.isValid() )
      return 0.0;
    return m_time_offset_seconds + m_end_time.toTime_t();
  }
  
  
  virtual double data( int row_raw, int column ) const
  {
    if( column < 0 || column > 5 )
      return NaN_VALUE;
    
    int row = row_raw / 2;
    
    if( column == 0 )
    {
      if( row_raw % 2 )
        row += 1;
      return m_time_offset_seconds + m_start_time.toTime_t() + (row + 0.5)*sm_nsec_interval;
    }
    
    // { 0: DateTime, 1: Oxygen, 2: HeartRate, 3: Moving: 4: Connected, 5: Battery  }
    
    const std::time_t interval_start = m_start_time.toTime_t() + row*sm_nsec_interval - (sm_nsec_interval/2);
    
    const WDateTime startTime = WDateTime::fromTime_t( interval_start );
    const WDateTime endTime = startTime.addSecs( sm_nsec_interval );
    
    std::lock_guard<mutex> data_lock( g_data_mutex );
    
    //Find range of data points between
    auto comp = []( const OwletReading &read, const WDateTime &dt ) -> bool {
      return read.utc_time < dt;
    };
    
    auto iter = std::lower_bound( begin(g_readings), end(g_readings), startTime, comp );
    
    int npoints = 0;
    double valsum = 0;
    
    for( ; (iter != end(g_readings)) && (iter->utc_time < endTime); ++iter )
    {
      const auto &reading = *iter;
      
      switch( column )
      {
        case 0: assert( 0 ); break;
        case 1:
          if( reading.oxygen > 0 )
          {
            ++npoints;
            valsum += reading.oxygen;
          }
          break;
          
        case 2:
          if( reading.heartrate > 0 )
          {
            ++npoints;
            valsum += reading.heartrate;
          }
          break;
          
        case 3:
          if( reading.movement > 0 )
            return static_cast<double>(reading.movement);
          break;
          
        case 4:
          if( reading.sock_connection > 0 )
            return static_cast<double>(reading.sock_connection);
          break;
          
        case 5:
          if( reading.battery > 0 )
            return static_cast<double>(reading.battery);
          break;
      }//switch( column )
    }//while( iterating over time interval )
    
    if( npoints )
      return std::round( valsum / npoints );
    return NaN_VALUE;
  }
  
  
  virtual void getAxisRange( const int column, const double x1, const double x2,
                                  double &ymin, double &ymax ) const
  {
    assert( column == 1 || column == 2 );
    
    
    if( column == 1 )
    {
      std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_oxygen_alarm.m_mutex );
      
      ymin = OwletWebApp::sm_oxygen_alarm.m_threshold - 4;
      
      ymax = 101;
    }else if( column == 2 )
    {
      {
        std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
        ymin = OwletWebApp::sm_heartrate_low_alarm.m_threshold - 5;
      }
      
      {
        std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_high_alarm.m_mutex );
        ymax = OwletWebApp::sm_heartrate_high_alarm.m_threshold + 5;
      }
    }//if( oxygen ) / else
    
    const int row_start = (x1 - m_time_offset_seconds - m_start_time.toTime_t()) / sm_nsec_interval;
    const int row_end = 1 + (x2 - m_time_offset_seconds - m_start_time.toTime_t()) / sm_nsec_interval;
    
    for( int row = row_start; row <= row_end; ++row )
    {
      const double o2value = data( row, column );
      ymin = std::min( ymin, o2value - ((column == 1) ? 1 : 5) );
    }
    
    //Lets have the lowest number always be even, so top number will be 100
    if( column == 1 )
      ymin -= ((static_cast<int>(ymin) % 2) ? 1 : 0);
  }//getAxisRange(...)
  
  
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
  

  virtual int columnCount() const
  {
    return 5;
  }
   
  virtual int rowCount() const
  {
    if( !m_start_time.isValid() || !m_end_time.isValid() )
      return 0;
    
    const int nsec = m_start_time.secsTo( m_end_time );
    const int nintervals = nsec / sm_nsec_interval;
    const int nextrasec = nsec % sm_nsec_interval;
    return 2*(nintervals + (nextrasec ? 1 : 0));
  }//
  
  //Additional functions we could use to customize the chart a little bit.
  //virtual const WColor *markerPenColor( int row, int column ) const
  //virtual const WColor *markerBrushColor (int row, int column) const
  //virtual const Chart::MarkerType *markerType( int row, int column) const
    
  //virtual const WColor *barPenColor (int row, int column) const
  //virtual const double *markerScaleFactor (int row, int column) const
};//class OwletDataPointModel


/**  A slight customization of WCartesianChart to allow drawing alarm limits and moving periods. */
class OwlChartImp : public Wt::Chart::WCartesianChart
{
  int m_lower_alarm, m_upper_alarm;
public:
  OwlChartImp()
  : WCartesianChart(),
    m_lower_alarm( 0 ),
    m_upper_alarm( 0 )
  {
  }
  
  
  
  void setAlarmLevels( const int lower, const int upper )
  {
    m_lower_alarm = lower;
    m_upper_alarm = upper;
    update();
  }
  
  
  virtual void paint( Wt::WPainter& painter, const WRectF& rectangle = WRectF() ) const override
  {
    WCartesianChart::paint( painter, rectangle );

    if( m_lower_alarm > 0 || m_upper_alarm > 0)
    {
      const auto oldpen = painter.pen();
      
      WColor pencolor("#B48EAD");
      pencolor.setRgb( pencolor.red(), pencolor.green(), pencolor.blue(), 125 );
      WPen newpen( pencolor );
      
      newpen.setStyle( PenStyle::DashLine );
      
      painter.setPen( newpen );
      
      auto &xaxis = axis(Wt::Chart::Axis::X);
      const double minx = xaxis.zoomMinimum();
      const double maxx = xaxis.zoomMaximum();
        
      if( m_lower_alarm > 0 )
        painter.drawLine( mapToDevice(minx, m_lower_alarm), mapToDevice( maxx, m_lower_alarm) );
      
      if( m_upper_alarm > 0 )
        painter.drawLine( mapToDevice(minx, m_upper_alarm), mapToDevice( maxx, m_upper_alarm) );
      
      painter.setPen( oldpen );
    }//if( have at least one alarm to draw )
  }//paint(...)
  
};//class OwlChartIm



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
: m_oxygen_chart( nullptr ), m_heartrate_chart( nullptr ), m_model( nullptr )
{
  addStyleClass( "OwletChart" );
  
  WGridLayout *layout = setLayout( make_unique<WGridLayout>() );
  
  m_model = make_shared<OwletDataPointModel>();
  m_oxygen_chart = layout->addWidget( std::make_unique<OwlChartImp>(), 0, 0, 1, 5 );
  m_heartrate_chart = layout->addWidget( std::make_unique<OwlChartImp>(), 1, 0, 1, 5 );
  
  Chart::WAxis &o2_xaxis = m_oxygen_chart->axis(Wt::Chart::Axis::X);
  Chart::WAxis &hr_xaxis = m_heartrate_chart->axis(Wt::Chart::Axis::X);
  
  o2_xaxis.zoomRangeChanged().connect( this, &OwletChart::zoomRangeChangeCallback );
  hr_xaxis.zoomRangeChanged().connect( this, &OwletChart::zoomRangeChangeCallback );
   
  configCharts();
  
  auto prev = std::make_unique<WImage>("assets/images/noun_previous_1897876.svg", "Previous Time Range");
  prev->addStyleClass( "PrevTimeRange" );
  
  auto next = std::make_unique<WImage>("assets/images/noun_Next_1897875.svg", "Next Time Range");
  next->addStyleClass( "NextTimeRange" );
  
  m_previous_range = layout->addWidget( std::move(prev), 2, 1, AlignmentFlag::Right );
  m_duration_select = layout->addWidget( make_unique<WComboBox>(), 2, 2, AlignmentFlag::Center | AlignmentFlag::Middle );
  m_next_range = layout->addWidget( std::move(next), 2, 3, AlignmentFlag::Left );
  
  m_previous_range->clicked().connect( this, &OwletChart::previousTimeRangeCallback );
  m_duration_select->activated().connect( this, &OwletChart::changeDurationCallback );
  m_next_range->clicked().connect( this, &OwletChart::nextTimeRangeCallback );
  
  
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
  
  layout->setColumnStretch( 0, 1 );
  layout->setColumnStretch( 4, 1 );
  layout->setRowStretch( 0, 1 );
  layout->setRowStretch( 1, 1 );
  
  
  // Set alarm thresholds to the chart
  int o2_threshold = 0, lower_hr_threshold = 0, upper_hr_threshold = 0;
  {
    std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_oxygen_alarm.m_mutex );
    o2_threshold = OwletWebApp::sm_oxygen_alarm.m_threshold;
  }

  {
    std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_low_alarm.m_mutex );
    lower_hr_threshold = OwletWebApp::sm_heartrate_low_alarm.m_threshold;
  }
  
  {
    std::lock_guard<mutex> alarm_lock( OwletWebApp::sm_heartrate_high_alarm.m_mutex );
    upper_hr_threshold = OwletWebApp::sm_heartrate_high_alarm.m_threshold;
  }
  
  m_oxygen_chart->setAlarmLevels( o2_threshold, 0 );
  m_heartrate_chart->setAlarmLevels( lower_hr_threshold, upper_hr_threshold );
  
  
  
  const double timezone_offset = 60*wApp->environment().timeZoneOffset().count();
  double end_time = WDateTime::currentDateTime().toTime_t() + timezone_offset;
  double start_time = end_time - 3*60*60;
  
  if( m_model->rowCount() )
  {
    end_time = m_model->maxDate();
    start_time = end_time - 3*3600;
    
    o2_xaxis.setZoomRange( start_time, end_time );
    hr_xaxis.setZoomRange( start_time, end_time );
    
    o2_xaxis.setMaximum( end_time + 3600 );
    hr_xaxis.setMaximum( end_time + 3600 );
  }//if( nrows )
  
  m_duration_select->setCurrentIndex( DurationIndex::ThreeHours );
  
  zoomRangeChangeCallback( start_time, end_time );
}//OwletChart()


void OwletChart::configCharts()
{
  const WColor chartBackgroundColor( "#ECEFF4" );
  const WPen axispen( WColor("#2E3440") );
  const WPen O2linepen( WColor("#5E81AC") );
  const WPen hrlinepen( WColor("#8FBCBB") );
  
  
  m_oxygen_chart->setPlotAreaPadding( 10, Side::Top );
  m_heartrate_chart->setPlotAreaPadding( 10, Side::Top );
  m_oxygen_chart->setPlotAreaPadding( 50, Side::Left );
  m_heartrate_chart->setPlotAreaPadding( 50, Side::Left );
  
  
  m_oxygen_chart->setAutoLayoutEnabled( true );
  m_heartrate_chart->setAutoLayoutEnabled( true );
  
  
  std::map<WFlags<KeyboardModifier>, Chart::InteractiveAction> wheelActions;
  wheelActions[KeyboardModifier::None] = Chart::InteractiveAction::PanX;
  wheelActions[KeyboardModifier::Control] = Chart::InteractiveAction::ZoomX;
  
  m_oxygen_chart->setWheelActions( wheelActions );
  m_heartrate_chart->setWheelActions( wheelActions );
  
  m_oxygen_chart->setModel( m_model );
  m_oxygen_chart->setXSeriesColumn(0);
  m_oxygen_chart->setType( Chart::ChartType::Scatter );
  m_oxygen_chart->setOnDemandLoadingEnabled( true );
  
  m_oxygen_chart->setBackground( chartBackgroundColor );
  
  m_heartrate_chart->setModel( m_model );
  m_heartrate_chart->setXSeriesColumn(0);
  m_heartrate_chart->setType( Chart::ChartType::Scatter );
  m_heartrate_chart->setOnDemandLoadingEnabled( true );
  m_heartrate_chart->setBackground( chartBackgroundColor );
  
  
  Chart::WAxis &o2_xaxis = m_oxygen_chart->axis(Wt::Chart::Axis::X);
  Chart::WAxis &hr_xaxis = m_heartrate_chart->axis(Wt::Chart::Axis::X);
  Chart::WAxis &o2_yaxis = m_oxygen_chart->axis(Wt::Chart::Axis::Y1);
  Chart::WAxis &hr_yaxis = m_heartrate_chart->axis(Wt::Chart::Axis::Y1);
  
  o2_xaxis.setScale(Chart::AxisScale::Date);
  o2_xaxis.setMinimumZoomRange(  15*60 );
  o2_xaxis.setMaximumZoomRange( 28*24*3600 );
  o2_xaxis.setScale( Chart::AxisScale::DateTime );
  
  hr_xaxis.setScale(Chart::AxisScale::Date);
  hr_xaxis.setMinimumZoomRange(  15*60 );
  hr_xaxis.setMaximumZoomRange( 28*24*3600 );
  hr_xaxis.setScale( Chart::AxisScale::DateTime );
    
  
  o2_yaxis.setTextPen( axispen );
  o2_xaxis.setTextPen( axispen );
  
  o2_xaxis.setPen( axispen );
  o2_xaxis.setTextPen( axispen );
  o2_yaxis.setPen( axispen );
  o2_yaxis.setTextPen( axispen );
  
  hr_xaxis.setPen( axispen );
  hr_xaxis.setTextPen( axispen );
  hr_yaxis.setPen( axispen );     //hrlinepen
  hr_yaxis.setTextPen( axispen ); //hrlinepen
  
  m_oxygen_chart->setTextPen( axispen );
  m_heartrate_chart->setTextPen( axispen );
  
  
  // Enable pan and zoom
  m_oxygen_chart->setPanEnabled( true );
  m_oxygen_chart->setZoomEnabled( true );
  m_oxygen_chart->setRubberBandEffectEnabled( true );
  
  // Enable pan and zoom
  m_heartrate_chart->setPanEnabled( true );
  m_heartrate_chart->setZoomEnabled( true );
  m_heartrate_chart->setRubberBandEffectEnabled( true );
  
  auto o2series = cpp14::make_unique<Chart::WDataSeries>(1, Chart::SeriesType::Line, Chart::Axis::Y1 );
  o2series->setPen( O2linepen );
  m_oxygen_chart->addSeries( std::move(o2series) );
  
  
  o2_yaxis.setRange( 90, 101 );
  o2_yaxis.setLabelInterval( 1 );
  o2_yaxis.setTitle( "Oxygen Saturation (%)" );
  o2_yaxis.setTitleOrientation(Orientation::Vertical);
  o2_yaxis.setVisible( true );
  
  auto hrseries = cpp14::make_unique<Chart::WDataSeries>(2, Chart::SeriesType::Line, Chart::Axis::Y1 );
  hrseries->setPen( hrlinepen );
  m_heartrate_chart->addSeries( std::move(hrseries) );
  

  //hr_yaxis.setRange( 40, 220 );
  //hr_yaxis.setLabelInterval( 20 );
  hr_yaxis.setAutoLimits( Chart::AxisValue::Both );
  hr_yaxis.setRoundLimits( Chart::AxisValue::Both );
  
  hr_yaxis.setTitle( "Heart Rate (BPM)" );
  hr_yaxis.setTitleOrientation(Orientation::Vertical);
  hr_yaxis.setVisible( true );
  
  m_oxygen_chart->setSeriesSelectionEnabled();
  m_heartrate_chart->setSeriesSelectionEnabled();
  
  m_oxygen_chart->setCrosshairEnabled( false );
  m_heartrate_chart->setCrosshairEnabled( false );
  
  //m_oxygen_chart->setFollowCurve( 1 );
  //m_heartrate_chart->setFollowCurve( 2 );
  
  //It looks like in JS we could listen for somethign like
  //  APP.emit(target.widget, "xTransformChanged");
  //
}//void configCharts()


bool OwletChart::isShowingFiveMinuteAvrg()
{
  auto ptr = dynamic_cast<const OwletDataPointModel *>( m_model.get() );
  return !ptr;
}//bool isShowingFiveMinuteAvrg()


void OwletChart::showFiveMinuteAvrgData()
{
  if( isShowingFiveMinuteAvrg() )
    return;
  
  m_model = make_shared<OwletFiveMinuteModel>();
  m_oxygen_chart->setModel( m_model );
  m_heartrate_chart->setModel( m_model );
  
  configCharts();
}//void showFiveMinuteAvrgData()


void OwletChart::showIndividualPointsData()
{
  if( !isShowingFiveMinuteAvrg() )
    return;
  
  m_model = make_shared<OwletDataPointModel>();
  m_oxygen_chart->setModel( m_model );
  m_heartrate_chart->setModel( m_model );
  
  configCharts();
}//void showIndividualPointsData()


void OwletChart::previousTimeRangeCallback()
{
  auto &o2_xaxis = m_oxygen_chart->axis(Wt::Chart::Axis::X);
  
  const double prev_start_time = o2_xaxis.zoomMinimum();
  const double prev_end_time = o2_xaxis.zoomMaximum();
  const double duration = prev_end_time - prev_start_time;
  
  const double data_start_time = m_model->minDate();
  
  double new_min = prev_start_time - duration;
  double new_max = prev_start_time;
  if( new_min < (data_start_time - 3600) )
  {
    new_min = data_start_time - 3600;
    new_max = new_min + duration;
  }
  
  o2_xaxis.setZoomRange( new_min, new_max );
  zoomRangeChangeCallback( new_min, new_max );
  
  m_next_range->enable();
  m_previous_range->setDisabled( (new_min <= data_start_time) );
}//void previousTimeRangeCallback()


void OwletChart::nextTimeRangeCallback()
{
  auto &o2_xaxis = m_oxygen_chart->axis(Wt::Chart::Axis::X);
  
  const double prev_start_time = o2_xaxis.zoomMinimum();
  const double prev_end_time = o2_xaxis.zoomMaximum();
  const double duration = prev_end_time - prev_start_time;
  const double data_end_time = m_model->maxDate();
  
  double new_min = prev_end_time;
  double new_max = prev_end_time + duration;
  if( new_max > (data_end_time + 3600) )
  {
    new_max = data_end_time + 3600;
    new_min = new_max - duration;
  }
  
  o2_xaxis.setZoomRange( new_min, new_max );
  zoomRangeChangeCallback( new_min, new_max );
  
  m_previous_range->setDisabled( false );
  m_next_range->setDisabled( (new_max >= data_end_time) );
}//void nextTimeRangeCallback()


void OwletChart::changeDurationCallback()
{
  const auto current = static_cast<DurationIndex>( m_duration_select->currentIndex() );
  double end_time = m_oxygen_chart->axis(Chart::Axis::X).zoomMaximum();
  
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
      start_time = m_oxygen_chart->axis(Chart::Axis::X).zoomMinimum();
      if( start_time > data_end_time )
          start_time = data_end_time - 12*60*60;
      break;
    case NumDurationIndexs:
      assert( 0 );
      break;
  }//switch (current )
  
  m_oxygen_chart->axis(Chart::Axis::X).setZoomRange( start_time, end_time );
  zoomRangeChangeCallback( start_time, end_time );
}//void changeDurationCallback()


void OwletChart::zoomRangeChangeCallback( double x1, double x2 )
{
  m_oxygen_chart->axis(Wt::Chart::Axis::X).setZoomRange( x1, x2 );
  m_heartrate_chart->axis(Wt::Chart::Axis::X).setZoomRange( x1, x2 );
  
  const double epsilon = 60;
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
  else
    cout << "Is custom duration of " << nseconds << " seconds" << endl;
  
  const auto current = static_cast<DurationIndex>( m_duration_select->currentIndex() );
  if( current != index )
    m_duration_select->setCurrentIndex( index );
  
  //Update the the enabled status of the previous/next buttons
  const int nrows = m_model->rowCount();
  if( nrows )
  {
    const double data_begin_time = m_model->data(0,0);
    m_previous_range->setDisabled( (x1 < data_begin_time) );
    
    const double data_end_time = m_model->data(nrows-1,0);
    m_next_range->setDisabled( (x2 >= data_end_time) );
  }else
  {
    m_previous_range->disable();
    m_next_range->disable();
  }
  
  //Now set the x-axis labels
  auto &o2_xaxis = m_oxygen_chart->axis(Wt::Chart::Axis::X);
  auto &hr_xaxis = m_heartrate_chart->axis(Wt::Chart::Axis::X);
  
  WDateTime today = WDateTime::currentDateTime();
  today = today.addSecs( 60*wApp->environment().timeZoneOffset().count() );
  today.setTime( WTime(0,0) );
  
  if( x2 < today.toTime_t() )
  {
    o2_xaxis.setLabelFormat("MM/dd hh:mm");
    hr_xaxis.setLabelFormat("MM/dd hh:mm");
  }else
  {
    o2_xaxis.setLabelFormat("hh:mm");
    hr_xaxis.setLabelFormat("hh:mm");
  }
  
  double ymin, ymax;
  m_model->getAxisRange( 1, x1, x2, ymin, ymax );
  m_oxygen_chart->axis(Wt::Chart::Axis::Y1).setRange( ymin, ymax );
  
  m_model->getAxisRange( 2, x1, x2, ymin, ymax );
  m_heartrate_chart->axis(Wt::Chart::Axis::Y1).setRange( ymin, ymax );
  
  /*
   // At the moment the rendering screws up if we set base point and such, so skip for now
  double label_interval = nseconds / 10;
  const int nminutes = nseconds / 60;
  auto lowerdt = WDateTime::fromTime_t( static_cast<std::time_t>(x1) );
  lowerdt = lowerdt.addSecs( 60 - lowerdt.time().second() ); //round up to the next minute
  
  if( nminutes < 20 )
  {
    //Find first even minute
    if( (lowerdt.time().minute() % 2) != 0 )
      lowerdt = lowerdt.addSecs( 60 );
    label_interval = 120;
  }else if( nminutes < 1*60 )
  {
    //Find first multiple of 5
    if( (lowerdt.time().minute() % 5) != 0 )
      lowerdt = lowerdt.addSecs( 300 - 60*(lowerdt.time().minute() % 5) );
    label_interval = 300;
  }else if( nminutes < 3*60 )
  {
    //Find first multiple of 15
    if( (lowerdt.time().minute() % 15) != 0 )
      lowerdt = lowerdt.addSecs( 15*60 - 60*(lowerdt.time().minute() % 15) );
    label_interval = 20*60;
  }else if( nminutes < 4*60 )
  {
    //Find first multiple of 20
    if( (lowerdt.time().minute() % 20) != 0 )
      lowerdt = lowerdt.addSecs( 20*60 - 60*(lowerdt.time().minute() % 20) );
    label_interval = 30*60;
  }else if( nminutes < 6*60 )
  {
    //Find first multiple of 30
    if( (lowerdt.time().minute() % 30) != 0 )
      lowerdt = lowerdt.addSecs( 30*60 - 60*(lowerdt.time().minute() % 30) );
    label_interval = 30*60;
  }else if( nminutes < (12*60 + 1) )
  {
    //Find first multiple of an hour
    if( (lowerdt.time().minute() % 60) != 0 )
      lowerdt = lowerdt.addSecs( 60*60 - 60*(lowerdt.time().minute() % 60) );
    label_interval = 60*60;
  }else if( nminutes < 3*24*60 )
  {
    //Find first multiple of an hour
    if( (lowerdt.time().minute() % 60) != 0 )
      lowerdt = lowerdt.addSecs( 60*60 - 60*(lowerdt.time().minute() % 60) );
    label_interval = 6*60*60;
  }else
  {
    //Find first day
    if( (lowerdt.time().minute() % 60) != 0 )
      lowerdt = lowerdt.addSecs( 60*60 - 60*(lowerdt.time().minute() % 60) );
    if( lowerdt.time().hour() != 0 )
      lowerdt = lowerdt.addSecs( 24*60*60 - 60*60*lowerdt.time().hour() - 60*lowerdt.time().minute() );
    label_interval = 60 * nminutes / 10; // \TODO: fix this
    label_interval = std::round(label_interval / 24 / 3600);
    if( label_interval < 1 )
      label_interval = 1;
    label_interval *= 24 * 2600;
  }//if( time range is this size ) / else ...

  
  cout << "label_interval=" + std::to_string(label_interval) +", (x2-x1)=" + std::to_string(x2-x1)
          + ", base point " + lowerdt.toString()
          + " where lower axis time is "
          + WDateTime::fromTime_t( static_cast<std::time_t>(x1) ).toString()
      << endl;
  
  o2_xaxis.setLabelBasePoint( static_cast<double>(lowerdt.toTime_t()) );
  o2_xaxis.setLabelInterval( label_interval );
  */
  
  
  //cout << "LAbel base point: " << lowerdt.toString() << " with inteval " << label_interval/60 << " minutes" << endl;
  //If upper limit is today, just display time, else display date.
  //m_oxygen_chart->axis(Wt::Chart::Axis::X).setLabelFormat(<#const WString &format#>);
}//void zoomRangeChangeCallback( double x1, double x2 )


void OwletChart::addedData( const size_t num_readings_before, const size_t num_readings_after )
{
  auto &o2_xaxis = m_oxygen_chart->axis(Wt::Chart::Axis::X);
  
  const double prev_max_date = m_model->maxDate();
  const double prev_max_x = o2_xaxis.zoomMaximum();
  const double prev_min_x = o2_xaxis.zoomMinimum();
  
  m_model->addedData( num_readings_before, num_readings_after );
  
  const double end_time = m_model->maxDate();
  o2_xaxis.setMaximum( end_time + 3600 );
  m_heartrate_chart->axis(Wt::Chart::Axis::X).setMaximum( end_time + 3600 );
  
  if( prev_max_x >= (prev_max_date - 60) )
  {
    const double new_x_end = std::max( end_time, end_time + (prev_max_x - prev_max_date) );
    const double new_x_min = new_x_end - (prev_max_x - prev_min_x);
    
    cout << "Old range = " << (prev_max_x - prev_min_x) << " new range = " << (new_x_end - new_x_min) << endl;
    zoomRangeChangeCallback( new_x_min, new_x_end );
  }//if( user could see most recent data )
}//void addedData( const size_t num_readings_before, const size_t num_readings_after )


void OwletChart::updateData( const vector<tuple<Wt::WDateTime,int,int,int>> &data )
{
  
}//void updateData( const vector<tuple<string,int,int,int>> &data )


void OwletChart::setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end )
{
  const double offset = 60*wApp->environment().timeZoneOffset().count();
  const double startTime = start.toTime_t() + offset;
  const double endTime = end.toTime_t() + offset;
  
  zoomRangeChangeCallback( startTime, endTime );
}//void setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end );

void OwletChart::oxygenAlarmLevelUpdated( const int o2level )
{
  m_oxygen_chart->setAlarmLevels( o2level, 0 );
}


void OwletChart::heartRateAlarmLevelUpdated( const int hrlower, const int hrupper )
{
  m_heartrate_chart->setAlarmLevels( hrlower, hrupper );
}

OwletChart::~OwletChart()
{
}

