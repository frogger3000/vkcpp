#include <chrono>
#include <deque>
#include <istream>
#include <memory>
#include <ostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace metrics {

enum class MetricType {
  INT,
  DOUBLE,
};

std::string format_metric_type(MetricType type);

using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

class MetricValue {
public:
  virtual void write_to_ostream(std::ostream &s) = 0;
  virtual void read_from_istream(std::istream &s) = 0;
  virtual MetricType type() = 0;
};

class IntMetric : virtual MetricValue {
  int value_;

public:
  void write_to_ostream(std::ostream &s) override;
  void read_from_istream(std::istream &s) override;
  MetricType type() override;
};
class DoubleMetric : virtual MetricValue {
  double value_;

public:
  void write_to_ostream(std::ostream &s) override;
  void read_from_istream(std::istream &s) override;
  MetricType type() override;
};

struct Metric {
  std::string name;
  std::shared_ptr<MetricValue> value;
  Timestamp timestamp;

  Metric(std::string name, std::shared_ptr<MetricValue> value,
         const Timestamp &timestamp);
  Metric(std::string name, std::shared_ptr<MetricValue> value,
         Timestamp &&timestamp);
};

class MetricCollector {
  std::unordered_map<std::string, std::pair<MetricType, std::vector<size_t>>>
      metrics_idxs_by_metric_type;

  std::vector<Metric> metrics;

  std::mutex metrics_mutex;
  std::atomic<bool> running;
  std::thread write_worker;
  std::string filename;

  std::chrono::duration<double> to_sleep;
  void worker();

public:
  MetricCollector(const std::string &filename,
                  std::chrono::duration<double> to_sleep);
  void AddMetricLock(const Metric &metric);
  void CreateMetricType(const std::string &name, MetricType type);
  void RemoveMetricType(const std::string &name);
  void AddMetricWithCurrentTimestamp(const std::string &name,
                                     std::shared_ptr<MetricValue> metric);
  void AddMetric(const std::string &name, std::shared_ptr<MetricValue> metric,
                 const Timestamp &timestamp);
  void AddMetric(const std::string &name, std::shared_ptr<MetricValue> metric,
                 Timestamp &&timestamp);
  void start();
  void stop();
};
} // namespace metrics
