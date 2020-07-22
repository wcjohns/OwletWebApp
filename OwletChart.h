#ifndef OwletChart_h
#define OwletChart_h

#include <tuple>
#include <vector>
#include <string>

#include <Wt/WDateTime.h>
#include <Wt/WContainerWidget.h>

class OwletChart : public Wt::WContainerWidget
{
  bool m_oxygen;
  bool m_heartrate;
  
  int m_last_oxygen;
  int m_last_heartrate;
  
public:
  OwletChart( const bool oxygen, const bool heartrate );
  void updateData( const std::vector<std::tuple<std::string,int,int,int>> &data );
  
  void setDateRange( const Wt::WDateTime &start, const Wt::WDateTime &end );
  
  virtual ~OwletChart();
};//class OwletChart

#endif
