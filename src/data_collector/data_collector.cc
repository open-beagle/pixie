#include <iostream>
#include <utility>

#include "src/data_collector/data_collector.h"
#include "src/data_collector/pub_sub_manager.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

DataCollector::DataCollector() { config_ = std::make_unique<PubSubManager>(schemas_); }

// Add an EBPF source to the Data Collector.
Status DataCollector::AddEBPFSource(const std::string& name, const std::string& ebpf_src,
                                    const std::string& kernel_event, const std::string& fn_name) {
  // Step 1: Create the Connector (with EBPF program attached).
  // TODO(kgandhi): This will come from the registry.
  std::vector<InfoClassElement> elements = {};
  auto source = std::make_unique<EBPFConnector>(name, elements, kernel_event, fn_name, ebpf_src);

  return AddSource(name, std::move(source));
}

// Add an OpenTracing source to the Data Collector.
Status DataCollector::AddOpenTracingSource(const std::string& name) {
  // Step 1: Create the Connector
  // TODO(kgandhi): This will come from the registry.
  std::vector<InfoClassElement> elements = {};
  auto source = std::make_unique<OpenTracingConnector>(name, elements);

  return AddSource(name, std::move(source));
}

Status DataCollector::AddSource(const std::string& name, std::unique_ptr<SourceConnector> source) {
  // Step 2: Ask the Connector for the Schema.
  // Eventually, should return a vector of Schemas.
  auto schema = std::make_unique<InfoClassSchema>(name);
  PL_RETURN_IF_ERROR(source->PopulateSchema(schema.get()));

  // Step 3: Make the corresponding Data Table.
  auto data_table = std::make_unique<ColumnWrapperDataTable>(*schema);

  // Step 4: Connect this Info Class to its related objects.
  schema->SetSourceConnector(source.get());
  schema->SetDataTable(data_table.get());

  // Step 5: Keep pointers to all the objects
  sources_.push_back(std::move(source));
  tables_.push_back(std::move(data_table));
  schemas_.push_back(std::move(schema));

  return Status::OK();
}

/**
 * Register call-back.
 */
void DataCollector::RegisterCallback(
    std::function<void(uint64_t,
                       std::unique_ptr<std::vector<std::shared_ptr<carnot::udf::ColumnWrapper>>>)>
        f) {
  agent_callback_ = f;
}

// Main call to start the data collection.
void DataCollector::Run() {
  run_thread_ = std::thread(&DataCollector::RunThread, this);
  // TODO(oazizi): Make sure this is not called multiple times...don't want thread proliferation.
}

void DataCollector::Wait() { run_thread_.join(); }

// Main Data Collector loop.
// Poll on Data Source Through connectors, when appropriate, then go to sleep.
// Must run as a thread, so only call from Run() as a thread.
void DataCollector::RunThread() {
  bool run = true;
  while (run) {
    // Run through every InfoClass being managed.
    for (const auto& schema : schemas_) {
      // Phase 1: Probe each source for its data.
      if (schema->SamplingRequired()) {
        auto source = schema->GetSourceConnector();
        auto data_table = schema->GetDataTable();

        // Get pointer to data.
        // Source manages its own buffer as appropriate.
        // For example, EBPFConnector may want to copy data to user-space,
        // and then provide a pointer to the data.
        // The complexity of re-using same memory buffer then falls to the Data Source.
        auto source_data = source->GetData();
        auto num_records = source_data.num_records;
        auto* data_buf = reinterpret_cast<uint8_t*>(source_data.buf);

        Status s;
        s = data_table->AppendData(data_buf, num_records);
        CHECK(s.ok());
      }

      // Phase 2: Push Data upstream.
      if (schema->PushRequired()) {
        auto data_table = schema->GetDataTable();

        // auto arrow_table = data_table->SealTableArrow();
        // PL_UNUSED(arrow_table);

        auto columns = data_table->SealTableColumnWrapper();
        PL_UNUSED(columns);

        // TODO(oazizi): Hook this up.
        // agent_callback_(schema->id(), std::move(columns));
      }

      // Optional: Update sampling periods if we are dropping data.
    }

    // Figure out how long to sleep.
    SleepUntilNextTick();

    // FIXME(oazizi): Remove this.
    std::cout << "." << std::flush;
  }
}

// Helper function: Figure out when to wake up next.
void DataCollector::SleepUntilNextTick() {
  // FIXME(oazizi): This is bogus.
  // The amount to sleep depends on when the earliest Source needs to be sampled again.
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

}  // namespace datacollector
}  // namespace pl
