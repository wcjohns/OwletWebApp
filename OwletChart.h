#ifndef OwletChart_h
#define OwletChart_h

#include <tuple>
#include <vector>
#include <string>

#include <Wt/WDateTime.h>
#include <Wt/WContainerWidget.h>

class OwlChartImp;
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
  OwlChartImp *m_oxygen_chart;
  OwlChartImp *m_heartrate_chart;
  std::shared_ptr<OwletChartModel> m_model;
  
  Wt::WComboBox *m_duration_select;
  Wt::WInteractWidget *m_previous_range;
  Wt::WInteractWidget *m_next_range;
  
  void configCharts();
  
public:
  OwletChart( const bool oxygen, const bool heartrate );
  void updateData( const std::vector<std::tuple<Wt::WDateTime,int,int,int>> &data );
  
  void setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end );
  
  
  void addedData( const size_t num_readings_before, const size_t num_readings_after );
  
  void previousTimeRangeCallback();
  void nextTimeRangeCallback();
  void changeDurationCallback();
  void zoomRangeChangeCallback( double x1, double x2 );
  
  bool isShowingFiveMinuteAvrg();
  void showFiveMinuteAvrgData();
  void showIndividualPointsData();
  
  void oxygenAlarmLevelUpdated( const int o2level );
  void heartRateAlarmLevelUpdated( const int hrlower, const int hrupper );
  
  virtual ~OwletChart();
};//class OwletChart

#endif
