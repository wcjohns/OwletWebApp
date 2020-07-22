#ifndef Alarmer_h
#define Alarmer_h

#include <mutex>
#include <memory>

#include <Wt/WDateTime.h>
#include <Wt/AsioWrapper/steady_timer.hpp>

//Forward declaration
struct AlarmerTester;

struct Alarmer
{
  Alarmer();
  ~Alarmer();
  void processData( const Wt::WDateTime &datetime, const int value, const bool final_data );
  void snooze_alarm();
  
  bool currently_alarming();
  bool currently_snoozed();
  
  bool enabled();
  void set_enabled( const bool enable );
  
  /** mutex to protect all member variables. */
  std::mutex m_mutex;
  
  /** The threshold must be equal to or beyond to start the countdown to notifying to the users. */
  int m_threshold;
  
  /** How many seconds the value must continously be deviant for, before the alarm starts going off. */
  int m_deviation_seconds;
  
  /** How many seconds the (user) snooze lasts for. */
  int m_snooze_seconds;
  
  /** Whether value must be above, or bellow #m_threshold for the alarming to start.   */
  bool m_lessthan;
  
  /** Function to call to start the alarm.  Set by client. */
  std::function<void(void)> m_start_alarm;
  
  /** Function to call to stop the alarm.  Set by client. */
  std::function<void(void)> m_stop_alarm;
  
  
protected:
  /** Whether alarming is enabledor not */
  bool m_enabled;
  
  /** The users have been notified of the alarm */
  bool m_alarm_notified;
  
  /** The time of the first deviation to the threshold.  If most recent value is not deviant, this time will be null.  */
  Wt::WDateTime m_deviation_start_time;
  
  /** Timer set when a deviatnt value is first seen (for #m_deviation_seconds) where if a non-deviant value is not seen before
   this timer goes off, then the alarm will be triggered.
   */
  std::unique_ptr<Wt::AsioWrapper::asio::steady_timer> m_alarm_wait_timer;
  
  /** Timer set when a user snoozes the alarm, and when it expires it will re-start the alarm, if any non-deviant values havent
   been seen since.
   */
  std::unique_ptr<Wt::AsioWrapper::asio::steady_timer> m_snooze_timer;
  
  friend struct AlarmerTester;
};//struct Alarmer


/** kinda like a poor-persons unit test for Alarmer. */
void test_alarmer( int argc, char **argv );

#endif //Alarmer_h
