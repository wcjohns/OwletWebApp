#ifndef Settings_h
#define Settings_h

#include <Wt/WDialog.h>
#include <Wt/WContainerWidget.h>

class Settings;

namespace Wt
{
  class WSpinBox;
  class WCheckBox;
}//namespace Wt

class SettingsDialog : public Wt::WDialog
{
public:
  SettingsDialog();
  
  Settings *m_settings;
};//class SettingsDialog



class Settings : public Wt::WContainerWidget
{
public:
  Settings();
  virtual ~Settings();
  
  void setThresholdsFromIniToGui();
  
  Wt::WCheckBox *m_fivemin_avrg;
  Wt::WSpinBox *m_oxygen_limit;
  Wt::WSpinBox *m_oxygen_time_wait;
  Wt::WSpinBox *m_low_heartrate_limit;
  Wt::WSpinBox *m_hearrate_time_wait;
  Wt::WSpinBox *m_high_heartrate_limit;
  Wt::WSpinBox *m_sock_off_wait;
};//class OwletChart

#endif //Settings_h

