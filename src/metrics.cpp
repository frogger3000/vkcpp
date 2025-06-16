#include "metrics.h"
#include "util.h"
#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <thread>

std::string metrics::format_metric_type(MetricType type) {
  switch (type) {
  case metrics::MetricType::DOUBLE:
    return "double";
  case metrics::MetricType::INT:
    return "int";
  }
}
void metrics::IntMetric::write_to_ostream(std::ostream &s) { s << value_; }
void metrics::IntMetric::read_from_istream(std::istream &s) { s >> value_; }
metrics::MetricType metrics::IntMetric::type() {
  return metrics::MetricType::INT;
}

void metrics::DoubleMetric::write_to_ostream(std::ostream &s) { s << value_; }
void metrics::DoubleMetric::read_from_istream(std::istream &s) { s >> value_; }
metrics::MetricType metrics::DoubleMetric::type() {
  return metrics::MetricType::DOUBLE;
}

void metrics::MetricCollector::AddMetricWithCurrentTimestamp(
    const std::string &name, std::shared_ptr<MetricValue> metric) {
  AddMetric(name, metric, std::move(std::chrono::system_clock::now()));
}

void metrics::MetricCollector::AddMetric(const std::string &name,
                                         std::shared_ptr<MetricValue> metric,
                                         const metrics::Timestamp &timestamp) {
  auto timestamp_copy = timestamp;
  AddMetric(name, metric, std::move(timestamp_copy));
}

metrics::Metric::Metric(std::string name, std::shared_ptr<MetricValue> value,
                        const Timestamp &timestamp)
    : name(name), value(value), timestamp(timestamp) {}
metrics::Metric::Metric(std::string name, std::shared_ptr<MetricValue> value,
                        Timestamp &&timestamp)
    : name(name), value(value), timestamp(timestamp) {}

void metrics::MetricCollector::AddMetricLock(const Metric &metric) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  auto metric_it = metrics_idxs_by_metric_type.find(metric.name);
  if (metric_it == metrics_idxs_by_metric_type.end()) {
    throw std::runtime_error(std::format("metric {} not found",
                                         metrics::utils::quoted(metric.name)));
  }

  auto &[type, idxs] = metric_it->second;
  auto metric_type = metric.value->type();
  if (type != metric_type) {
    throw std::runtime_error(
        std::format("metric of type {} cannot be added for metric {} with type",
                    metrics::format_metric_type(metric_type),
                    metrics::utils::quoted(metric.name),
                    metrics::format_metric_type(type)));
  }
  idxs.push_back(metrics.size());
  metrics.push_back(metric);
}

void metrics::MetricCollector::AddMetric(const std::string &name,
                                         std::shared_ptr<MetricValue> metric,
                                         Timestamp &&timestamp) {

  std::thread t([](metrics::MetricCollector *c,
                   metrics::Metric &&metric) { c->AddMetricLock(metric); },
                this, std::move(Metric(name, metric, timestamp)));
}

void metrics::MetricCollector::CreateMetricType(const std::string &name,
                                                MetricType type) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  if (metrics_idxs_by_metric_type.contains(name)) {
    throw std::runtime_error(std::format("metric with name {} already exists",
                                         metrics::utils::quoted(name)));
  }
  metrics_idxs_by_metric_type.insert({name, {type, {}}});
}
void metrics::MetricCollector::RemoveMetricType(const std::string &name) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  if (!metrics_idxs_by_metric_type.contains(name)) {
    throw std::runtime_error(std::format("metric with name {} does not exists",
                                         metrics::utils::quoted(name)));
  }

  for (auto &[metric, type_idx_pair] : metrics_idxs_by_metric_type) {
    auto &[type, idxs] = type_idx_pair;
    idxs.clear();
  }

  std::vector<Metric> new_metrics;
  for (auto &metric : metrics) {
    if (metric.name == name) {
      continue;
    }
    metrics_idxs_by_metric_type[metric.name].second.push_back(
        new_metrics.size());
    new_metrics.push_back(metric);
  }
  metrics = new_metrics;
}

void metrics::MetricCollector::start() {
  running = true;
  std::thread t([](MetricCollector *m) { m->worker(); }, this);
  t.join();
}
void metrics::MetricCollector::stop() { running = false; }

metrics::MetricCollector::MetricCollector(
    const std::string &filename, std::chrono::duration<double> to_sleep)
    : filename(filename), to_sleep(to_sleep) {}

void metrics::MetricCollector::worker() {
  std::ofstream outfile;
  outfile.open(filename, std::ios_base::app);
  if (!outfile.is_open()) {
    throw std::runtime_error(std::format("could not open file {} for appending",
                                         metrics::utils::quoted(filename)));
  }
  while (running) {
    std::this_thread::sleep_for(to_sleep);
    {
      std::lock_guard<std::mutex> lock(metrics_mutex);
      for (auto &metric : metrics) {
        outfile << std::format("{:%D %T}", metric.timestamp) << " "
                << std::quoted(metric.name) << " ";
        metric.value->write_to_ostream(outfile);
        outfile << "\n";
      }
      outfile.flush();
      metrics.clear();
      for (auto &[metric, type_idx_pair] : metrics_idxs_by_metric_type) {
        auto &[type, idxs] = type_idx_pair;
        idxs.clear();
      }
    }
  }
}
