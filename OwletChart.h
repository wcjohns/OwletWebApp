#ifndef OwletChart_h
#define OwletChart_h

#include <tuple>
#include <vector>
#include <string>

#include <Wt/WDateTime.h>
#include <Wt/WContainerWidget.h>

class OwletChartModel;

namespace Wt{
namespace Chart{
class WCheckBox;
class WComboBox;
class WPushButton;
class WCartesianChart;

} }

class OwletChart : public Wt::WContainerWidget
{
  Wt::Chart::WCartesianChart *m_chart;
  std::shared_ptr<OwletChartModel> m_model;
  
  Wt::WComboBox *m_duration_select;
  Wt::WPushButton *m_previous_range;
  Wt::WPushButton *m_next_range;
  Wt::WCheckBox *m_oxygen;
  Wt::WCheckBox *m_heartrate;
  
  int m_last_oxygen;
  int m_last_heartrate;
  
public:
  OwletChart( const bool oxygen, const bool heartrate );
  void updateData( const std::vector<std::tuple<Wt::WDateTime,int,int,int>> &data );
  
  void setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end );
  
  
  void addedData( const size_t num_readings_before, const size_t num_readings_after );
  
  void previousTimeRangeCallback();
  void nextTimeRangeCallback();
  void changeDurationCallback();
  void zoomRangeChangeCallback( double x1, double x2 );
  
  virtual ~OwletChart();
};//class OwletChart

#endif
