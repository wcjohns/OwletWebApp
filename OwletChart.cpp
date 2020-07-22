#include <tuple>
#include <vector>
#include <string>

#include <Wt/WDateTime.h>
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>

#include "Database.h"
#include "OwletChart.h"


using namespace std;
using namespace Wt;


OwletChart::OwletChart( const bool oxygen, const bool heartrate )
: m_oxygen( oxygen ), m_heartrate( heartrate )
{
  WApplication::instance()->useStyleSheet( "assets/css/dygraph.min.css" );
  WApplication::instance()->require( "assets/js/dygraph.min.js" );
  
  addStyleClass( "OwletChart" );
  
  m_last_oxygen = 100;
  m_last_heartrate = 100;
  
  //We'll use WDateTime to make sure data is sorted, and to combine
  //  instances of when we have both oxygen and heartrate.
  //  I couldnt get all the UTC->local->UTC working right, so we'll
  //  just use strings for datetime
  map<WDateTime,tuple<string,int,int,int>> data;
  
  {//begin lock on g_data_mutex
    std::lock_guard<mutex> data_lock( g_data_mutex );
    
    if( m_oxygen )
    {
      for( const auto &v : g_oxygen_values )
      {
        try
        {
          const string &timestr = get<0>(v);
          const auto date = WDateTime::fromString(timestr,"yyyy-MM-ddThh:mm:ssZ");
          if( date.isNull() || !date.isValid() )
            throw runtime_error( "Failed to convert oxygen time '" + timestr + "'" );
          
          auto &val = data[date];
          get<0>(val) = timestr;
          get<1>(val) = get<1>(v);
          get<3>(val) = get<2>(v);
        }catch( std::exception &e )
        {
          cerr << "OwletChart constructor caught: " << e.what() << endl;
        }
      }//for( const auto &v : g_oxygen_values )
    }//if( oxygen )
    
    if( m_heartrate )
    {
      for( const auto &v : g_heartrate_values )
      {
        const string &timestr = get<0>(v);
        const auto date = WDateTime::fromString(timestr,"yyyy-MM-ddThh:mm:ssZ");
        if( date.isNull() || !date.isValid() )
          throw runtime_error( "Failed to convert heartrate time '" + timestr + "'" );
        
        auto &val = data[date];
        get<0>(val) = timestr;
        get<2>(val) = get<1>(v);
        get<3>(val) = get<2>(v);
      }//for( const auto &v : g_heartrate_values )
    }//if( heartrate )
  }//end lock on g_data_mutex
  
  int iter = 0;
  WStringStream datastrm;
  datastrm << "[";
  for( const auto &val : data )
  {
    const string &time = get<0>(val.second);
    const int o2 = get<1>(val.second);
    const int heart = get<2>(val.second);
    
    if( (!m_heartrate && !o2) || (!m_oxygen && !heart) )  //shouldnt be necassary, but JIC
      continue;
    
    //datastrm << "+\"" << val.first.toString("yyyy-MM-dd hh:mm:ss",false).toUTF8() << ",";
    if( iter++ )
      datastrm << ",";
    datastrm << "[new Date(\"" << time << "\")";
    if( m_oxygen )
      datastrm << "," << (o2 ? o2 : m_last_oxygen);
    if( m_heartrate )
      datastrm << "," << (heart ? heart : m_last_heartrate);
    datastrm << "]";
    
    m_last_oxygen = (o2 ? o2 : m_last_oxygen);
    m_last_heartrate = (heart ? heart : m_last_heartrate);
  }//for( const auto &val : data )
  
  
  datastrm << "];";
  
  //cout << datastrm.str() << endl;
  setJavaScriptMember( "data", datastrm.str() );
  
  string options = "{ drawPoints: true";
  //options += ", showRangeSelector: true";
  //, animatedZooms: true
  //, rangeSelectorHeight: 30, rangeSelectorPlotStrokeColor: 'yellow', rangeSelectorPlotFillColor: 'lightyellow',
  options += ", labels: ['Time'";
  if( m_oxygen )
    options += ", 'Oxygen'";
  if( m_heartrate )
    options += ", 'HeartRate'";
  options += "], ";
  
  if( m_oxygen && m_heartrate )
    options += "legend: 'always',";
  
  if( m_oxygen && m_heartrate )
    options += "series: {'HeartRate': { axis: 'y2' } }, ";
  options += "axes: {"
  "y: { axisLabelWidth: 60, drawGrid: true, independentTicks: true";
  if( m_oxygen )
    options += ", valueRange: [80,102]";
  options += "}";
  if( m_oxygen && m_heartrate )
    options += ", y2: { labelsKMB: true, independentTicks: false}";
  options += "}";
  if( m_oxygen )
    options += ", ylabel: 'Oxygen Level (%)'";
  else if( m_heartrate )
    options += ", ylabel: 'HeartRate (BPM)'";
  if( m_oxygen && m_heartrate )
    options += ", y2label: 'HeartRate (BPM)'";
  //options += ", visibility: [true, false]";
  options += "}";
  
  //cout << "options=" << options << endl;
  
  //TODO: Fix legend display - maybe add a div to the right, which
  //      should also have the radio buttons to toggle what is shown.
  //      see: http://dygraphs.com/options.html#valueFormatter
  //      and http://dygraphs.com/tests/legend-formatter.html
  
  setJavaScriptMember( "chart", "new Dygraph(" + jsRef() + "," + jsRef() + ".data," + options + ");");
  
  string resizejs = "setTimeout(function(){ window.dispatchEvent(new Event('resize'));}, 250 );";
  doJavaScript( resizejs );
  resizejs = "setTimeout(function(){ window.dispatchEvent(new Event('resize'));}, 1000 );";
  doJavaScript( resizejs );
  resizejs = "setTimeout(function(){ window.dispatchEvent(new Event('resize'));}, 2000 );";
  doJavaScript( resizejs );
  
  if( data.size() )
  {
    const auto last_iter = data.rbegin();
    const WDateTime &last_time = last_iter->first;
    setDateRange( last_time.addSecs(-12*60*60), last_time );
  }
}//Dygraph()


void OwletChart::updateData( const vector<tuple<string,int,int,int>> &data )
{
  size_t nentries = 0;
  WStringStream datastrm;
  WDateTime last_02, last_heartrate;
  
  for( const auto &val : data )
  {
    const string &datetime = get<0>(val);
    const int o2 = get<1>(val);
    const int heart = get<2>(val);
    //const int movement = get<3>(val.second);
    
    if( (m_heartrate && heart) || (m_oxygen && o2) )
    {
      datastrm << jsRef() << ".data.push([new Date(\"" << datetime << "\")";
      if( m_oxygen )
        datastrm << "," << (o2 ? o2 : m_last_oxygen);
      if( m_heartrate )
        datastrm << "," << (heart ? heart : m_last_heartrate);
      datastrm << "]);\n";
      ++nentries;
    }//if( (m_heartrate && heart) || (m_oxygen && o2) )
    
    m_last_oxygen = (o2 ? o2 : m_last_oxygen);
    m_last_heartrate = (heart ? heart : m_last_heartrate);
    
    last_02 = o2 ? WDateTime::fromString(datetime,"yyyy-MM-ddThh:mm:ssZ") : last_02;
    last_heartrate = heart ? WDateTime::fromString(datetime,"yyyy-MM-ddThh:mm:ssZ") : last_heartrate;
  }//for( const auto &val : data )
  
  if( !nentries )
    return;
  
  //cout << datastrm.str() << endl;
  datastrm << jsRef() << ".chart.updateOptions( { 'file': " <<  jsRef() << ".data } );";
  doJavaScript( datastrm.str() );
}//void updateData( const vector<tuple<string,int,int,int>> &data )


void OwletChart::setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end )
{
  if( !start.isValid() || !end.isValid() || end <= start )
    throw runtime_error( "OwletChart::setDateRange(): invalid input" );
  
  
  const string startstr = start.toString("yyyy-MM-ddThh:mm:ss",false).toUTF8() + "Z";
  const string endstr = start.toString("yyyy-MM-ddThh:mm:ss",false).toUTF8() + "Z";
  
  //const string js = jsRef() + ".chart.updateOptions({ axes: { x: { "
  //           "valueRange: [new Date(\"" + startstr + "\"),new Date(\"" + endstr + "\")] } } });";
  //doJavaScript( js );
  
  const string js = jsRef() + ".chart.updateOptions({ axes: { x: { "
             "dateWindow: [new Date(\"" + startstr + "\"),new Date(\"" + endstr + "\")] } } });";
  doJavaScript( js );
  
}//void setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end );


OwletChart::~OwletChart()
{
}

