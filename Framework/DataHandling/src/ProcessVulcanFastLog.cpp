#include "MantidDataHandling/ProcessVulcanFastLog.h"
#include "MantidAPI/AnalysisDataService.h"
#include "MantidAPI/Axis.h"
#include "MantidAPI/FileFinder.h"
#include "MantidAPI/FileProperty.h"
#include "MantidAPI/RegisterFileLoader.h"
#include "MantidAPI/Run.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidDataObjects/EventList.h"
#include "MantidDataObjects/EventWorkspace.h"
#include "MantidDataObjects/Workspace2D.h"
#include "MantidGeometry/IDetector.h"
#include "MantidGeometry/Instrument.h"
#include "MantidGeometry/Instrument/DetectorInfo.h"
#include "MantidKernel/ArrayProperty.h"
#include "MantidKernel/BinaryFile.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/CPUTimer.h"
#include "MantidKernel/ConfigService.h"
#include "MantidKernel/DateAndTime.h"
#include "MantidKernel/FileValidator.h"
#include "MantidKernel/Glob.h"
#include "MantidKernel/InstrumentInfo.h"
#include "MantidKernel/ListValidator.h"
#include "MantidKernel/OptionalBool.h"
#include "MantidKernel/System.h"
#include "MantidKernel/TimeSeriesProperty.h"
#include "MantidKernel/UnitFactory.h"
#include "MantidKernel/VisibleWhenProperty.h"

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <boost/timer.hpp>

#include <Poco/File.h>
#include <Poco/Path.h>

#include <H5Cpp.h>

namespace Mantid {
namespace DataHandling {

DECLARE_FILELOADER_ALGORITHM(ProcessVulcanFastLog)

using namespace Kernel;
using namespace API;
using namespace Geometry;
using namespace DataObjects;

using boost::posix_time::ptime;
using boost::posix_time::time_duration;
using DataObjects::EventList;
using DataObjects::EventWorkspace;
using DataObjects::EventWorkspace_sptr;
using DataObjects::TofEvent;
using std::cout;
using std::ifstream;
using std::runtime_error;
using std::stringstream;
using std::string;
using std::vector;

////------------------------------------------------------------------------------------------------
//// constants for locating the parameters to use in execution
////------------------------------------------------------------------------------------------------
static const string EVENT_PARAM("EventFilename");
static const string PULSEID_PARAM("PulseidFilename");
static const string MAP_PARAM("MappingFilename");
static const string PID_PARAM("SpectrumList");
static const string PARALLEL_PARAM("UseParallelProcessing");
static const string BLOCK_SIZE_PARAM("LoadingBlockSize");
static const string OUT_PARAM("OutputWorkspace");

/// All pixel ids with matching this mask are errors.
static const PixelType ERROR_PID = 0x80000000;
/// The maximum possible tof as native type
static const uint32_t MAX_TOF_UINT32 = std::numeric_limits<uint32_t>::max();
/// Conversion factor between 100 nanoseconds and 1 microsecond.
static const double TOF_CONVERSION = .1;
/// Conversion factor between picoColumbs and microAmp*hours
static const double CURRENT_CONVERSION = 1.e-6 / 3600.;
/// Veto flag: 0xFF00000000000
static const uint64_t VETOFLAG(72057594037927935);

static const string EVENT_EXTS[] = {
    "_neutron_event.dat",     "_neutron0_event.dat", "_neutron1_event.dat",
    "_neutron2_event.dat",    "_neutron3_event.dat", "_neutron4_event.dat",
    "_live_neutron_event.dat"};
static const string PULSE_EXTS[] = {
    "_pulseid.dat",  "_pulseid0.dat", "_pulseid1.dat",    "_pulseid2.dat",
    "_pulseid3.dat", "_pulseid4.dat", "_live_pulseid.dat"};
static const int NUM_EXT = 7;

////-----------------------------------------------------------------------------
//// Statistic Functions
////-----------------------------------------------------------------------------

////----------------------------------------------------------------------------------------------
///** Parse preNexus file name to get run number
//  */
// static string getRunnumber(const string &filename);
static string getRunnumber(const string &filename) {
  // start by trimming the filename
  string runnumber(Poco::Path(filename).getBaseName());

  if (runnumber.find("neutron") >= string::npos)
    return "0";

  std::size_t left = runnumber.find('_');
  std::size_t right = runnumber.find('_', left + 1);

  return runnumber.substr(left + 1, right - left - 1);
}

////----------------------------------------------------------------------------------------------
///** Generate Pulse ID file name from preNexus event file's name
//  */
// static string generatePulseidName(string eventfile);
static string generatePulseidName(string eventfile) {
  // initialize vector of endings and put live at the beginning
  vector<string> eventExts(EVENT_EXTS, EVENT_EXTS + NUM_EXT);
  std::reverse(eventExts.begin(), eventExts.end());
  vector<string> pulseExts(PULSE_EXTS, PULSE_EXTS + NUM_EXT);
  std::reverse(pulseExts.begin(), pulseExts.end());

  // look for the correct ending
  for (std::size_t i = 0; i < eventExts.size(); ++i) {
    size_t start = eventfile.find(eventExts[i]);
    if (start != string::npos)
      return eventfile.replace(start, eventExts[i].size(), pulseExts[i]);
  }

  // give up and return nothing
  return "";
}

////----------------------------------------------------------------------------------------------
///** Generate mapping file name from Event workspace's instrument
//  */
// static string generateMappingfileName(EventWorkspace_sptr &wksp);
static string generateMappingfileName(EventWorkspace_sptr &wksp) {
  // get the name of the mapping file as set in the parameter files
  std::vector<string> temp =
      wksp->getInstrument()->getStringParameter("TS_mapping_file");
  if (temp.empty())
    return "";
  string mapping = temp[0];
  // Try to get it from the working directory
  Poco::File localmap(mapping);
  if (localmap.exists())
    return mapping;

  // Try to get it from the data directories
  string dataversion = Mantid::API::FileFinder::Instance().getFullPath(mapping);
  if (!dataversion.empty())
    return dataversion;

  // get a list of all proposal directories
  string instrument = wksp->getInstrument()->getName();
  Poco::File base("/SNS/" + instrument + "/");
  // try short instrument name
  if (!base.exists()) {
    instrument =
        Kernel::ConfigService::Instance().getInstrument(instrument).shortName();
    base = Poco::File("/SNS/" + instrument + "/");
    if (!base.exists())
      return "";
  }
  vector<string> dirs; // poco won't let me reuse temp
  base.list(dirs);

  // check all of the proposals for the mapping file in the canonical place
  const string CAL("_CAL");
  const size_t CAL_LEN = CAL.length(); // cache to make life easier
  vector<string> files;
  for (auto &dir : dirs) {
    if ((dir.length() > CAL_LEN) &&
        (dir.compare(dir.length() - CAL.length(), CAL.length(), CAL) == 0)) {
      std::string path = std::string(base.path())
                             .append("/")
                             .append(dir)
                             .append("/calibrations/")
                             .append(mapping);
      if (Poco::File(path).exists())
        files.push_back(path);
    }
  }

  if (files.empty())
    return "";
  else if (files.size() == 1)
    return files[0];
  else // just assume that the last one is the right one, this should never be
       // fired
    return *(files.rbegin());
}

//----------------------------------------------------------------------------------------------
/** Return the confidence with with this algorithm can load the file
 *  @param descriptor A descriptor for the file
 *  @returns An integer specifying the confidence level. 0 indicates it will
 * not
 * be used
 */
int ProcessVulcanFastLog::confidence(Kernel::FileDescriptor &descriptor) const {
  if (descriptor.extension().rfind("dat") == std::string::npos)
    return 0;

  // If this looks like a binary file where the exact file length is a
  // multiple
  // of the DasEvent struct then we're probably okay.
  if (descriptor.isAscii())
    return 0;

  const size_t objSize = sizeof(DasEvent);
  auto &handle = descriptor.data();
  // get the size of the file in bytes and reset the handle back to the
  // beginning
  handle.seekg(0, std::ios::end);
  const size_t filesize = static_cast<size_t>(handle.tellg());
  handle.seekg(0, std::ios::beg);

  if (filesize % objSize == 0)
    return 80;
  else
    return 0;
}

//----------------------------------------------------------------------------------------------
/** Constructor
 */
ProcessVulcanFastLog::ProcessVulcanFastLog()
    : Mantid::API::IFileLoader<Kernel::FileDescriptor>(), prog(nullptr),
      spectra_list(), pulsetimes(), event_indices(), proton_charge(),
      proton_charge_tot(0), pixel_to_wkspindex(), pixelmap(), detid_max(),
      eventfile(nullptr), num_events(0), num_pulses(0), numpixel(0),
      num_good_events(0), num_error_events(0), num_bad_events(0),
      num_wrongdetid_events(0), num_ignored_events(0), first_event(0),
      max_events(0), using_mapping_file(false), loadOnlySomeSpectra(false),
      spectraLoadMap(), longest_tof(0), shortest_tof(0),
      parallelProcessing(false), pulsetimesincreasing(false), m_dbOutput(false),
      m_dbOpBlockNumber(0), m_dbOpNumEvents(0), m_dbOpNumPulses(0) {}

//----------------------------------------------------------------------------------------------
/** Desctructor
 */
ProcessVulcanFastLog::~ProcessVulcanFastLog() { delete this->eventfile; }

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm, i.e, declare properties
*/
void ProcessVulcanFastLog::init() {
  // which files to use
  vector<string> eventExts(EVENT_EXTS, EVENT_EXTS + NUM_EXT);
  declareProperty(
      Kernel::make_unique<FileProperty>(EVENT_PARAM, "", FileProperty::Load,
                                        eventExts),
      "The name of the neutron event file to read, including its full or "
      "relative path. In most cases, the file typically ends in "
      "neutron_event.dat (N.B. case sensitive if running on Linux).");
  vector<string> pulseExts(PULSE_EXTS, PULSE_EXTS + NUM_EXT);
  declareProperty(Kernel::make_unique<FileProperty>(
                      PULSEID_PARAM, "", FileProperty::OptionalLoad, pulseExts),
                  "File containing the accelerator pulse information; the "
                  "filename will be found automatically if not specified.");
  declareProperty(
      Kernel::make_unique<FileProperty>(MAP_PARAM, "",
                                        FileProperty::OptionalLoad, ".dat"),
      "File containing the pixel mapping (DAS pixels to pixel IDs) file "
      "(typically INSTRUMENT_TS_YYYY_MM_DD.dat). The filename will be found "
      "automatically if not specified.");

  // which pixels to load
  declareProperty(Kernel::make_unique<ArrayProperty<int64_t>>(PID_PARAM),
                  "A list of individual spectra (pixel IDs) to read, specified "
                  "as e.g. 10:20. Only used if set.");

  auto mustBePositive = boost::make_shared<BoundedValidator<int>>();
  mustBePositive->setLower(1);
  declareProperty("ChunkNumber", EMPTY_INT(), mustBePositive,
                  "If loading the file by sections ('chunks'), this is the "
                  "section number of this execution of the algorithm.");
  declareProperty("TotalChunks", EMPTY_INT(), mustBePositive,
                  "If loading the file by sections ('chunks'), this is the "
                  "total number of sections.");
  // TotalChunks is only meaningful if ChunkNumber is set
  // Would be nice to be able to restrict ChunkNumber to be <= TotalChunks at
  // validation
  setPropertySettings("TotalChunks", make_unique<VisibleWhenProperty>(
                                         "ChunkNumber", IS_NOT_DEFAULT));

  std::vector<std::string> propOptions{"Auto", "Serial", "Parallel"};
  declareProperty("UseParallelProcessing", "Auto",
                  boost::make_shared<StringListValidator>(propOptions),
                  "Use multiple cores for loading the data?\n"
                  "  Auto: Use serial loading for small data sets, parallel "
                  "for large data sets.\n"
                  "  Serial: Use a single core.\n"
                  "  Parallel: Use all available cores.");

  // Group of properties for FAST LOG --------------------------------------
  // the output workspace name
  declareProperty("MinimumPixelID", EMPTY_INT(), mustBePositive,
                  "Minmum pixel IDs for the sample log to export.");

  declareProperty("MaximumPixelID", EMPTY_INT(), mustBePositive,
                  "Maximum pixel IDs for the sample log to export.");

  declareProperty(
      Kernel::make_unique<WorkspaceProperty<MatrixWorkspace>>(
          OUT_PARAM, "", Direction::Output),
      "The name of the workspace that contains the fast sample log.");

  // TODO - Define workspace 2

  auto mustBeNonNegative = boost::make_shared<BoundedValidator<int>>();
  mustBeNonNegative->setLower(0);
  declareProperty("DBOutputBlockNumber", EMPTY_INT(), mustBePositive,
                  "Index of the loading block for debugging output. ");

  std::string fastloggrp("Fast Sample Log To Invtestigate");
  setPropertyGroup("MinimumPixelID", fastloggrp);
  setPropertyGroup("MaximumPixelID", fastloggrp);
  setPropertyGroup(OUT_PARAM, fastloggrp);
  setPropertyGroup("DBOutputBlockNumber", fastloggrp);
  //------------------------------------------------------------------------

  // Some debugging options

  declareProperty("DBNumberOutputEvents", 40, mustBePositive,
                  "Number of output events for debugging purpose.  Must be "
                  "defined with DBOutputBlockNumber.");

  declareProperty("DBNumberOutputPulses", EMPTY_INT(), mustBePositive,
                  "Number of output pulses for debugging purpose. ");

  std::string dbgrp = "Investigation Use";

  setPropertyGroup("DBNumberOutputEvents", dbgrp);
  setPropertyGroup("DBNumberOutputPulses", dbgrp);

  declareProperty("BlockNumberForLog", 0, "Index of block to investigate");
  declareProperty("EventRangeForLog", 0,
                  "Range of events in a block for investigating logs");

  std::string loggrp = "Investigate Event-based Sample Logs";
  setPropertyGroup("BlockNumberForLog", loggrp);
  setPropertyGroup("EventRangeForLog", loggrp);
}

//------------------------------------------------------------------------------------------------
/**
 * @brief ProcessVulcanFastLog::processInputs
 */
void ProcessVulcanFastLog::processInputs() {
  // TODO/ISSUE/NOW - Put it to some other more appropriate places
  // FAST-LOG-FLAG
  int max_pixel_id_i = getProperty("MaximumPixelID");
  if (isEmpty(max_pixel_id_i))
    max_pixel_id_i = 1610730000;
  m_maxLogPixelID = static_cast<size_t>(max_pixel_id_i);

  int min_pixel_id_i = getProperty("MinimumPixelID");
  if (isEmpty(min_pixel_id_i))
    min_pixel_id_i = detid_max + 1;
  m_minLogPixelID = static_cast<size_t>(min_pixel_id_i);

  if (m_minLogPixelID >= m_maxLogPixelID) {
    g_log.error() << "detid max = " << detid_max << "\n";
    throw std::runtime_error("Min and Max Pixel ID are not defined correctly.");
  }
}

//----------------------------------------------------------------------------------------------
/** Execute the algorithm
  * Procedure:
  * 1. check all the inputs
  * 2. create an EventWorkspace object
  * 3. process events
  * 4. set out output
  */
void ProcessVulcanFastLog::exec() {
  g_log.information("Executing Process Vulcan Fast Log");

  // test writing H5 file
  // exportTimeSeriesH5("/tmp/test.h5", 50);

  // Process input properties
  // a. Check 'chunk' properties are valid, if set
  const int chunks = getProperty("TotalChunks");
  if (!isEmpty(chunks) && int(getProperty("ChunkNumber")) > chunks) {
    throw std::out_of_range("ChunkNumber cannot be larger than TotalChunks");
  }

  prog = make_unique<Progress>(this, 0.0, 1.0, 100);

  // b. what spectra (pixel ID's) to load
  this->spectra_list = this->getProperty(PID_PARAM);

  // c. the event file is needed in case the pulseid fileanme is empty
  string event_filename = this->getPropertyValue(EVENT_PARAM);
  string pulseid_filename = this->getPropertyValue(PULSEID_PARAM);
  bool throwError = true;
  if (pulseid_filename.empty()) {
    pulseid_filename = generatePulseidName(event_filename);
    if (!pulseid_filename.empty()) {
      if (Poco::File(pulseid_filename).exists()) {
        this->g_log.information() << "Found pulseid file " << pulseid_filename
                                  << '\n';
        throwError = false;
      } else {
        pulseid_filename = "";
      }
    }
  }

  processInputs();

  g_log.warning() << "Processing investigration inputs."
                  << "\n";
  processInvestigationInputs();

  // Read input files
  prog->report("Loading Pulse ID file");
  this->readPulseidFile(pulseid_filename, throwError);
  prog->report("Loading Event File");
  g_log.warning() << "About to opening event file " << event_filename << "\n";
  this->openEventFile(event_filename);

  // Correct event indexes mased by veto flag
  unmaskVetoEventIndex();

  // Create otuput Workspace
  g_log.warning() << "About to create output workspaces"
                  << "\n";
  prog->report("Creating output workspace");
  createEventWorkspace(event_filename);

  // Process the events into pixels
  procEvents(localWorkspace);

  // Set output
  //  this->setProperty<IEventWorkspace_sptr>(OUT_PARAM, localWorkspace);

  createOutputs();

  // Fast frequency sample environment data
  //  this->processImbedLogs();

} // exec()

//------------------------------------------------------------------------------------------------
/** Create and set up output Event Workspace
  */
void ProcessVulcanFastLog::createEventWorkspace(
    const std::string event_filename) {

  // TODO/NOW/ISSUE/ -- create vector
  std::vector<std::vector<std::pair<int64_t, size_t>>>
      m_signalBlockVector; // blocks and entries

  // Create the output workspace
  localWorkspace = EventWorkspace_sptr(new EventWorkspace());

  // Make sure to initialize. We can use dummy numbers for arguments, for
  // event
  // workspace it doesn't matter
  localWorkspace->initialize(1, 1, 1);

  // Set the units
  localWorkspace->getAxis(0)->unit() = UnitFactory::Instance().create("TOF");
  localWorkspace->setYUnit("Counts");

  // Set title
  localWorkspace->setTitle("Dummy Title");

  // Property run_start
  if (this->num_pulses > 0) {
    // add the start of the run as a ISO8601 date/time string. The start = the
    // first pulse.
    // (this is used in LoadInstrument to find the right instrument file to
    // use).
    localWorkspace->mutableRun().addProperty(
        "run_start", pulsetimes[0].toISO8601String(), true);
  }

  // Property run_number
  localWorkspace->mutableRun().addProperty("run_number",
                                           getRunnumber(event_filename));

  // Get the instrument!
  prog->report("Loading Instrument");
  this->runLoadInstrument(event_filename, localWorkspace);

  // load the mapping file
  prog->report("Loading Mapping File");
  string mapping_filename = this->getPropertyValue(MAP_PARAM);
  if (mapping_filename.empty()) {
    mapping_filename = generateMappingfileName(localWorkspace);
    if (!mapping_filename.empty())
      this->g_log.information() << "Found mapping file \"" << mapping_filename
                                << "\"\n";
  }
  this->loadPixelMap(mapping_filename);

  // Replace workspace by workspace of correct size
  // Number of non-monitors in instrument
  size_t nSpec = localWorkspace->getInstrument()->getDetectorIDs(true).size();
  if (!this->spectra_list.empty())
    nSpec = this->spectra_list.size();
  auto tmp = createWorkspace<EventWorkspace>(nSpec, 2, 1);
  WorkspaceFactory::Instance().initializeFromParent(*localWorkspace, *tmp,
                                                    true);
  localWorkspace = std::move(tmp);
}

//------------------------------------------------------------------------------------------------
/** Some Pulse ID and event indexes might be wrong.  Remove them.
  */
void ProcessVulcanFastLog::unmaskVetoEventIndex() {
  // Unmask veto bit from vetoed events

  PARALLEL_FOR_NO_WSP_CHECK()
  for (int i = 0; i < static_cast<int>(event_indices.size()); ++i) {
    PARALLEL_START_INTERUPT_REGION

    uint64_t eventindex = event_indices[i];
    if (eventindex > static_cast<uint64_t>(max_events)) {
      // Is veto, use the unmasked event index
      uint64_t realeventindex = eventindex & VETOFLAG;
      event_indices[i] = realeventindex;
    }

    // Check
    uint64_t eventindexcheck = event_indices[i];
    if (eventindexcheck > static_cast<uint64_t>(max_events)) {
      g_log.information() << "Check: Pulse " << i
                          << ": unphysical event index = " << eventindexcheck
                          << "\n";
    }
    PARALLEL_END_INTERUPT_REGION
  }
  PARALLEL_CHECK_INTERUPT_REGION
}

//------------------------------------------------------------------------------------------------
/** Generate a workspace with distribution of events with pulse
  * Workspace has 2 spectrum.  spectrum 0 is the number of events in one
 * pulse.
  * specrum 1 is the accumulated number of events
  */
API::MatrixWorkspace_sptr
ProcessVulcanFastLog::generateEventDistribtionWorkspace() {
  // Generate workspace of 2 spectrum
  size_t nspec = 2;
  size_t sizex = event_indices.size();
  size_t sizey = sizex;
  MatrixWorkspace_sptr disws = boost::dynamic_pointer_cast<MatrixWorkspace>(
      WorkspaceFactory::Instance().create("Workspace2D", nspec, sizex, sizey));

  g_log.debug() << "Event indexes size = " << event_indices.size() << ", "
                << "Number of pulses = " << pulsetimes.size() << "\n";

  // Put x-values
  for (size_t i = 0; i < 2; ++i) {
    auto &dataX = disws->mutableX(i);
    dataX[0] = 0;
    for (size_t j = 0; j < sizex; ++j) {
      int64_t time =
          pulsetimes[j].totalNanoseconds() - pulsetimes[0].totalNanoseconds();
      dataX[j] = static_cast<double>(time) * 1.0E-9;
    }
  }

  // Put y-values
  auto &dataY0 = disws->mutableY(0);
  auto &dataY1 = disws->mutableY(1);

  dataY0[0] = 0;
  dataY1[1] = static_cast<double>(event_indices[0]);

  for (size_t i = 1; i < sizey; ++i) {
    dataY0[i] = static_cast<double>(event_indices[i] - event_indices[i - 1]);
    dataY1[i] = static_cast<double>(event_indices[i]);
  }

  return disws;
}

//----------------------------------------------------------------------------------------------
/** Process imbed logs (marked by bad pixel IDs)
 */
void ProcessVulcanFastLog::processImbedLogs() {
  for (const auto pid : this->wrongdetids) {
    // a. pixel ID -> index
    const auto mit = this->wrongdetidmap.find(pid);
    size_t mindex = mit->second;
    if (mindex > this->wrongdetid_pulsetimes.size()) {
      g_log.error() << "Wrong Index " << mindex << " for Pixel " << pid << '\n';
      throw std::invalid_argument("Wrong array index for pixel from map");
    } else {
      g_log.information() << "Processing imbed log marked by Pixel " << pid
                          << " with size = "
                          << this->wrongdetid_pulsetimes[mindex].size() << '\n';
    }

    std::stringstream ssname;
    ssname << "Pixel" << pid;
    std::string logname = ssname.str();

    // d. Add this to log
    this->addToWorkspaceLog(logname, mindex);

    g_log.notice() << "Processed imbedded log " << logname << "\n";

  } // ENDFOR pit
}

//----------------------------------------------------------------------------------------------
/** Add absolute time series to log. Use TOF as log value for this type of
 * events
  * @param logtitle :: name of the log
  * @param mindex :: index of the log in pulse time ...
  * - mindex:  index of the the series in the list
  */
void ProcessVulcanFastLog::addToWorkspaceLog(std::string logtitle,
                                             size_t mindex) {
  // Create TimeSeriesProperty
  auto property = new TimeSeriesProperty<double>(logtitle);

  // Add entries
  size_t nbins = this->wrongdetid_pulsetimes[mindex].size();
  for (size_t k = 0; k < nbins; k++) {
    double tof = this->wrongdetid_tofs[mindex][k];
    DateAndTime pulsetime = wrongdetid_pulsetimes[mindex][k];
    int64_t abstime_ns =
        pulsetime.totalNanoseconds() + static_cast<int64_t>(tof * 1000);
    DateAndTime abstime(abstime_ns);
    property->addValue(abstime, tof);
  } // ENDFOR

  // Add property to workspace
  localWorkspace->mutableRun().addProperty(property, false);

  g_log.information() << "Size of Property " << property->name() << " = "
                      << property->size() << " vs Original Log Size = " << nbins
                      << "\n";
}

//----------------------------------------------------------------------------------------------
/** Load the instrument geometry File
 *  @param eventfilename :: Used to pick the instrument.
 *  @param localWorkspace :: MatrixWorkspace in which to put the instrument
 * geometry
 */
void ProcessVulcanFastLog::runLoadInstrument(
    const std::string &eventfilename, MatrixWorkspace_sptr localWorkspace) {
  // start by getting just the filename
  string instrument = Poco::Path(eventfilename).getFileName();

  // initialize vector of endings and put live at the beginning
  vector<string> eventExts(EVENT_EXTS, EVENT_EXTS + NUM_EXT);
  std::reverse(eventExts.begin(), eventExts.end());

  for (const auto &ending : eventExts) {
    size_t pos = instrument.find(ending);
    if (pos != string::npos) {
      instrument = instrument.substr(0, pos);
      break;
    }
  }

  // determine the instrument parameter file
  size_t pos = instrument.rfind('_'); // get rid of the run number
  instrument = instrument.substr(0, pos);

  // do the actual work
  IAlgorithm_sptr loadInst = createChildAlgorithm("LoadInstrument");

  // Now execute the Child Algorithm. Catch and log any error, but don't stop.
  loadInst->setPropertyValue("InstrumentName", instrument);
  loadInst->setProperty<MatrixWorkspace_sptr>("Workspace", localWorkspace);
  loadInst->setProperty("RewriteSpectraMap",
                        Mantid::Kernel::OptionalBool(false));
  loadInst->executeAsChildAlg();

  // Populate the instrument parameters in this workspace - this works around
  // a
  // bug
  localWorkspace->populateInstrumentParameters();
}

//----------------------------------------------------------------------------------------------
/** Turn a pixel id into a "corrected" pixelid and period.
  *
  */
inline void ProcessVulcanFastLog::fixPixelId(PixelType &pixel,
                                             uint32_t &period) const {
  if (!this->using_mapping_file) { // nothing to do here
    period = 0;
    return;
  }

  PixelType unmapped_pid = pixel % this->numpixel;
  period = (pixel - unmapped_pid) / this->numpixel;
  pixel = this->pixelmap[unmapped_pid];
}

//----------------------------------------------------------------------------------------------
/** Process the event file properly in parallel
  * @param workspace :: EventWorkspace to write to.
  */
void ProcessVulcanFastLog::procEvents(
    DataObjects::EventWorkspace_sptr &event_workspace) {
  //-------------------------------------------------------------------------
  // Initialize statistic counters
  //-------------------------------------------------------------------------
  this->num_error_events = 0;
  this->num_good_events = 0;
  this->num_ignored_events = 0;
  this->num_bad_events = 0;
  this->num_wrongdetid_events = 0;

  shortest_tof = static_cast<double>(MAX_TOF_UINT32) * TOF_CONVERSION;
  longest_tof = 0.;

  // Set up loading parameters
  size_t loadBlockSize = Mantid::Kernel::DEFAULT_BLOCK_SIZE * 2;
  size_t numBlocks = (max_events + loadBlockSize - 1) / loadBlockSize;

  g_log.notice() << "Number of blocks: " << numBlocks << "\n";

  m_timeBlockVector.resize(numBlocks);
  m_valueBlockVector.resize(numBlocks);

  // We want to pad out empty pixels.
  const auto &detectorInfo = event_workspace->detectorInfo();
  const auto &detIDs = detectorInfo.detectorIDs();

  // Determine processing mode
  std::string procMode = getProperty("UseParallelProcessing");
  if (procMode == "Serial")
    parallelProcessing = false;
  else if (procMode == "Parallel")
    parallelProcessing = true;
  else {
    // Automatic determination. Loading serially (for me) is about 3 million
    // events per second,
    // (which is sped up by ~ x 3 with parallel processing, say 10 million per
    // second, e.g. 7 million events more per seconds).
    // compared to a setup time/merging time of about 10 seconds per million
    // detectors.
    double setUpTime = double(detectorInfo.size()) * 10e-6;
    parallelProcessing = ((double(max_events) / 7e6) > setUpTime);
    g_log.debug() << (parallelProcessing ? "Using" : "Not using")
                  << " parallel processing.\n";
  }

  // determine maximum pixel id
  detid_max = 0; // seems like a safe lower bound
  for (const auto detID : detIDs)
    if (detID > detid_max)
      detid_max = detID;

  g_log.warning() << "Dector ID max = " << detid_max << "\n";

  // For slight speed up
  loadOnlySomeSpectra = (!this->spectra_list.empty());

  // Turn the spectra list into a map, for speed of access
  for (auto &spectrum : spectra_list)
    spectraLoadMap[spectrum] = true;

  // Pad all the pixels
  prog->report("Padding Pixels");
  this->pixel_to_wkspindex.reserve(
      detid_max + 1); // starting at zero up to and including detid_max
  // Set to zero
  this->pixel_to_wkspindex.assign(detid_max + 1, 0);
  size_t workspaceIndex = 0;
  specnum_t spectrumNumber = 1;
  for (size_t i = 0; i < detectorInfo.size(); ++i) {
    if (!detectorInfo.isMonitor(i)) {
      if (!loadOnlySomeSpectra ||
          (spectraLoadMap.find(detIDs[i]) != spectraLoadMap.end())) {
        this->pixel_to_wkspindex[detIDs[i]] = workspaceIndex;
        EventList &spec = event_workspace->getSpectrum(workspaceIndex);
        spec.setDetectorID(detIDs[i]);
        spec.setSpectrumNo(spectrumNumber);
        ++workspaceIndex;
      } else {
        this->pixel_to_wkspindex[detIDs[i]] = -1;
      }
      ++spectrumNumber;
    }
  }

  CPUTimer tim;

  //-------------------------------------------------------------------------
  // Create the partial workspaces
  //-------------------------------------------------------------------------
  // Vector of partial workspaces, for parallel processing.
  std::vector<EventWorkspace_sptr> partWorkspaces;
  std::vector<DasEvent *> buffers;

  /// Pointer to the vector of events
  typedef std::vector<TofEvent> *EventVector_pt;
  /// Bare array of arrays of pointers to the EventVectors
  EventVector_pt **eventVectors;

  /// How many threads will we use?
  size_t numThreads = 1;
  if (parallelProcessing)
    numThreads = size_t(PARALLEL_GET_MAX_THREADS);

  partWorkspaces.resize(numThreads);
  buffers.resize(numThreads);
  eventVectors = new EventVector_pt *[numThreads];

  // cppcheck-suppress syntaxError
    PRAGMA_OMP( parallel for if (parallelProcessing) )
    for (int i = 0; i < int(numThreads); i++) {
      // This is the partial workspace we are about to create (if in parallel)
      EventWorkspace_sptr partWS;
      if (parallelProcessing) {
        prog->report("Creating Partial Workspace");
        // Create a partial workspace, copy all the spectra numbers and stuff
        // (no actual events to copy though).
        partWS = event_workspace->clone();
        // Push it in the array
        partWorkspaces[i] = partWS;
      } else
        partWS = event_workspace;

      // Allocate the buffers
      buffers[i] = new DasEvent[loadBlockSize];

      // For each partial workspace, make an array where index = detector ID and
      // value = pointer to the events vector
      eventVectors[i] = new EventVector_pt[detid_max + 1];
      EventVector_pt *theseEventVectors = eventVectors[i];
      for (detid_t j = 0; j < detid_max + 1; j++) {
        size_t wi = pixel_to_wkspindex[j];
        // Save a POINTER to the vector<tofEvent>
        if (wi != static_cast<size_t>(-1))
          theseEventVectors[j] = &partWS->getSpectrum(wi).getEvents();
        else
          theseEventVectors[j] = nullptr;
      }
    }

    g_log.warning()
        << tim << " to create " << partWorkspaces.size()
        << " workspaces (same as number of threads) for parallel loading "
        << numBlocks << " blocks. "
        << "\n";

    prog->resetNumSteps(numBlocks, 0.1, 0.8);

    //-------------------------------------------------------------------------
    // LOAD THE DATA
    //-------------------------------------------------------------------------

    PRAGMA_OMP( parallel for schedule(dynamic, 1) if (parallelProcessing) )
    for (int blockNum = 0; blockNum < int(numBlocks); blockNum++) {
      PARALLEL_START_INTERUPT_REGION

      // Find the workspace for this particular thread
      EventWorkspace_sptr ws;
      size_t threadNum = 0;
      if (parallelProcessing) {
        threadNum = PARALLEL_THREAD_NUMBER;
        ws = partWorkspaces[threadNum];
      } else
        ws = event_workspace;

      // Get the buffer (for this thread)
      DasEvent *event_buffer = buffers[threadNum];

      // Get the speeding-up array of vector<tofEvent> where index = detid.
      EventVector_pt *theseEventVectors = eventVectors[threadNum];

      // Where to start in the file?
      size_t fileOffset = first_event + (loadBlockSize * blockNum);
      // May need to reduce size of last (or only) block
      size_t current_event_buffer_size =
          (blockNum == int(numBlocks - 1))
              ? (max_events - (numBlocks - 1) * loadBlockSize)
              : loadBlockSize;

      // Load this chunk of event data (critical block)
      PARALLEL_CRITICAL(ProcessVulcanFastLog_fileAccess) {
        current_event_buffer_size = eventfile->loadBlockAt(
            event_buffer, fileOffset, current_event_buffer_size);
      }

      // This processes the events. Can be done in parallel!
      bool dbprint = m_dbOutput && (blockNum == m_dbOpBlockNumber);

      // sample log
      if (blockNum == m_logBlockNumber) {
        dbprint = true;
        g_log.notice() << "Block " << blockNum << " output = " << dbprint
                       << "\n";
      }

      std::vector<Kernel::DateAndTime> log_time_vector;
      std::vector<int64_t> log_value_vector;

      // THIS IS THE CORE METHOD TO PROCESS EVENTS
      procEventsLinear(ws, theseEventVectors, event_buffer,
                       current_event_buffer_size, fileOffset, log_time_vector,
                       log_value_vector, dbprint);

      m_timeBlockVector[blockNum] = log_time_vector;
      m_valueBlockVector[blockNum] = log_value_vector;

      // Report progress
      prog->report("Load Event PreNeXus");

      PARALLEL_END_INTERUPT_REGION
    }
    PARALLEL_CHECK_INTERUPT_REGION

    g_log.debug() << tim << " to load the data.\n";

    //-------------------------------------------------------------------------
    // MERGE WORKSPACES BACK TOGETHER
    //-------------------------------------------------------------------------
    // FIXME/NOW/LATER - Temporarily disable parallel processing
    parallelProcessing = false;
    if (parallelProcessing) {
      PARALLEL_START_INTERUPT_REGION
      prog->resetNumSteps(event_workspace->getNumberHistograms(), 0.8, 0.95);

      // Merge all workspaces, index by index.
      PARALLEL_FOR_NO_WSP_CHECK()
      for (int iwi = 0; iwi < int(event_workspace->getNumberHistograms());
           iwi++) {
        size_t wi = size_t(iwi);

        // The output event list.
        EventList &el = event_workspace->getSpectrum(wi);
        el.clear(false);

        // How many events will it have?
        size_t numEvents = 0;
        for (size_t i = 0; i < numThreads; i++)
          numEvents += partWorkspaces[i]->getSpectrum(wi).getNumberEvents();
        // This will avoid too much copying.
        el.reserve(numEvents);

        // Now merge the event lists
        for (size_t i = 0; i < numThreads; i++) {
          EventList &partEl = partWorkspaces[i]->getSpectrum(wi);
          el += partEl.getEvents();
          // Free up memory as you go along.
          partEl.clear(false);
        }
        prog->report("Merging Workspaces");
      }
      g_log.debug() << tim << " to merge workspaces together.\n";
      PARALLEL_END_INTERUPT_REGION
    }
    PARALLEL_CHECK_INTERUPT_REGION

    //-------------------------------------------------------------------------
    // Clean memory
    //-------------------------------------------------------------------------

    // Delete the buffers for each thread.
    for (size_t i = 0; i < numThreads; i++) {
      delete[] buffers[i];
      delete[] eventVectors[i];
    }
    delete[] eventVectors;
    // delete [] pulsetimes;

    prog->resetNumSteps(3, 0.94, 1.00);

    //-------------------------------------------------------------------------
    // Finalize loading
    //-------------------------------------------------------------------------
    prog->report("Setting proton charge");
    this->setProtonCharge(event_workspace);
    g_log.debug() << tim << " to set the proton charge log."
                  << "\n";

    // Make sure the MRU is cleared
    event_workspace->clearMRU();

    // Now, create a default X-vector for histogramming, with just 2 bins.
    if (false) {
      // FIXME/NOW - since there is no event, then this step is skipped
      auto axis = HistogramData::BinEdges{shortest_tof - 1, longest_tof + 1};
      event_workspace->setAllX(axis);
      this->pixel_to_wkspindex.clear();
    }

    /* Disabled! Final process on wrong detector id events
    for (size_t vi = 0; vi < this->wrongdetid_abstimes.size(); vi ++){
      std::sort(this->wrongdetid_abstimes[vi].begin(),
    this->wrongdetid_abstimes[vi].end());
    }
    */

    //-------------------------------------------------------------------------
    // Final message output
    //-------------------------------------------------------------------------
    g_log.notice() << "Read " << this->num_good_events << " events + "
                   << this->num_error_events << " errors"
                   << ". Shortest TOF: " << shortest_tof
                   << " microsec; longest TOF: " << longest_tof << " microsec."
                   << "\n"
                   << "Bad Events = " << this->num_bad_events
                   << "  Events of Wrong Detector = "
                   << this->num_wrongdetid_events << ", "
                   << "Number of Wrong Detector IDs = "
                   << this->wrongdetids.size() << "\n";

    std::set<PixelType>::iterator wit;
    for (wit = this->wrongdetids.begin(); wit != this->wrongdetids.end();
         ++wit) {
      g_log.notice() << "Wrong Detector ID : " << *wit << '\n';
    }
    std::map<PixelType, size_t>::iterator git;
    for (git = this->wrongdetidmap.begin(); git != this->wrongdetidmap.end();
         ++git) {
      PixelType tmpid = git->first;
      size_t vindex = git->second;
      g_log.notice() << "Pixel " << tmpid << ":  Total number of events = "
                     << this->wrongdetid_pulsetimes[vindex].size() << '\n';
    }

    //-------------------------------------------------------------------------
    // Information about logs
    //-------------------------------------------------------------------------
    g_log.notice() << "Number of blocks = " << m_timeBlockVector.size() << ", "
                   << m_valueBlockVector.size() << "\n";
    size_t num_entries = 0;
    for (size_t iblock = 0; iblock < m_timeBlockVector.size(); ++iblock)
      num_entries += m_timeBlockVector[iblock].size();
    g_log.notice() << "Total " << num_entries << " entries"
                   << "\n";

} // End of procEvents

//----------------------------------------------------------------------------------------------
/** Linear-version of the procedure to process the event file properly.
  * @param workspace :: EventWorkspace to write to.
  * @param arrayOfVectors :: For speed up: this is an array, of size
 * detid_max+1, where the
  *        index is a pixel ID, and the value is a pointer to the
 * vector<tofEvent> in the given EventList.
  * @param event_buffer :: The buffer containing the DAS events
  * @param current_event_buffer_size :: The length of the given DAS buffer
  * @param fileOffset :: Value for an offset into the binary file
  * @param dbprint :: flag to print out events information
  */
void ProcessVulcanFastLog::procEventsLinear(
    DataObjects::EventWorkspace_sptr & /*workspace*/,
    std::vector<TofEvent> **arrayOfVectors, DasEvent *event_buffer,
    size_t current_event_buffer_size, size_t fileOffset,
    std::vector<Kernel::DateAndTime> &log_time_vector,
    std::vector<int64_t> &log_value_vector, bool dbprint) {
  // Starting pulse time
  DateAndTime pulsetime;
  int64_t pulse_i = 0;
  int64_t numPulses = static_cast<int64_t>(num_pulses);
  if (event_indices.size() < num_pulses) {
    g_log.warning()
        << "Event_indices vector is smaller than the pulsetimes array.\n";
    numPulses = static_cast<int64_t>(event_indices.size());
  }

  // Local stastic parameters
  size_t local_num_error_events = 0;
  size_t local_num_bad_events = 0;
  size_t local_num_wrongdetid_events = 0;
  size_t local_num_ignored_events = 0;
  size_t local_num_good_events = 0;
  double local_shortest_tof =
      static_cast<double>(MAX_TOF_UINT32) * TOF_CONVERSION;
  double local_longest_tof = 0.;

  // Storages
  std::map<PixelType, size_t> local_pidindexmap;
  std::vector<std::vector<Kernel::DateAndTime>> local_pulsetimes;
  std::vector<std::vector<double>> local_tofs;
  std::set<PixelType> local_wrongdetids;

  // process the individual events
  std::stringstream dbss;
  // size_t numwrongpid = 0;

  size_t wrong_pixel_counter = 0;
  // TODO/ISSUE/NOW - Make it flexible!
  size_t log_output_counter = 0;
  size_t max_output_log_entries = 2000;

  size_t num_signal_block = 0; // number of signal counts in a block
  for (size_t i = 0; i < current_event_buffer_size; i++) {
    DasEvent &temp = *(event_buffer + i);
    PixelType pid = temp.pid;

    // Filter out bad event
    if ((pid & ERROR_PID) == ERROR_PID) {
      local_num_error_events++;
      local_num_bad_events++;
      continue;
    }

    // I don' think that the following codes are needed in this case
    if (false) {
      // Covert the pixel ID from DAS pixel to our pixel ID
      if (pid == 1073741843) {
        // convert a special PID to another special
        // downstream monitor pixel for SNAP
        pid = 1179648;
      } else if (this->using_mapping_file) {
        // using map file ... not used at all!
        PixelType unmapped_pid = pid % this->numpixel;
        pid = this->pixelmap[unmapped_pid];
      }
    }

    bool islog(false);
    bool iswrongdetid(false);

    // Sample log ID or WRONG ID Wrong pixel IDs
    if (pid > static_cast<PixelType>(detid_max)) {
      // PID is not detector: 2 cases (special event log interested or wrong!)

      if (pid >= static_cast<PixelType>(m_minLogPixelID) &&
          pid <= static_cast<PixelType>(m_maxLogPixelID)) {
        islog = true;
        ++num_signal_block;
      } else {
        iswrongdetid = true;

        local_num_error_events++;
        local_num_wrongdetid_events++;
        local_wrongdetids.insert(pid);
      }

      // some debug output
      if (dbprint && islog & log_output_counter < max_output_log_entries) {
        g_log.warning() << "Log ID " << pid << "\n";
      } else if (iswrongdetid) {
        ++wrong_pixel_counter;
      }
    } // END-IF (pid is not detector IDs)

    // Now check if this pid we want to load.
    if (loadOnlySomeSpectra && !iswrongdetid) {
      std::map<int64_t, bool>::iterator it;
      it = spectraLoadMap.find(pid);
      if (it == spectraLoadMap.end()) {
        // Pixel ID was not found, so the event is being ignored.
        local_num_ignored_events++;
        continue;
      }
    }

    // Upon this point, only 'good' events and log events are left to work on

    // Pulse: Find the pulse time for this event index
    if (pulse_i < numPulses - 1) {
      // This is the total offset into the file
      size_t total_i = i + fileOffset;
      // Go through event_index until you find where the index increases to
      // encompass the current index.
      // Your pulse = the one before.
      while (!((total_i >= event_indices[pulse_i]) &&
               (total_i < event_indices[pulse_i + 1]))) {
        pulse_i++;
        if (pulse_i >= (numPulses - 1))
          break;
      }

      // Save the pulse time at this index for creating those events
      pulsetime = pulsetimes[pulse_i];
    } // Find pulse time

    // TOF
    double tof = static_cast<double>(temp.tof) * TOF_CONVERSION;

    if (!iswrongdetid && !islog) {
      // Regular event that is belonged to a defined detector
      // Find the overall max/min tof
      if (tof < local_shortest_tof)
        local_shortest_tof = tof;
      if (tof > local_longest_tof)
        local_longest_tof = tof;

      // This is equivalent to
      // workspace->getSpectrum(this->pixel_to_wkspindex[pid]).addEventQuickly(event);
      // But should be faster as a bunch of these calls were cached.
      arrayOfVectors[pid]->emplace_back(tof, pulsetime);
      ++local_num_good_events;
    } else if (islog) {
      //  Special events
      Kernel::DateAndTime log_time(pulsetime.totalNanoseconds() +
                                   static_cast<int64_t>(tof * 1000));
      log_time_vector.push_back(log_time);
      log_value_vector.push_back(pid);
    } else {
      // Wrong detector id
      // i.  get/add index of the entry in map
      std::map<PixelType, size_t>::iterator it;
      it = local_pidindexmap.find(pid);
      size_t theindex = 0;
      if (it == local_pidindexmap.end()) {
        // Initialize it!
        size_t newindex = local_pulsetimes.size();
        local_pidindexmap[pid] = newindex;

        std::vector<Kernel::DateAndTime> tempvectime;
        std::vector<double> temptofs;
        local_pulsetimes.push_back(tempvectime);
        local_tofs.push_back(temptofs);

        theindex = newindex;

        // ++ numwrongpid;

        g_log.debug() << "Find New Wrong Pixel ID = " << pid << "\n";
      } else {
        // existing
        theindex = it->second;
      }

      // ii. calculate and add absolute time
      // int64_t abstime = (pulsetime.totalNanoseconds()+int64_t(tof*1000));
      local_pulsetimes[theindex].push_back(pulsetime);
      local_tofs[theindex].push_back(tof);

      if (dbprint) {
        int64_t abstime =
            pulsetime.totalNanoseconds() + static_cast<int64_t>(tof * 1000);
        g_log.information() << "LogTime  " << abstime << "\n";
      }

    } // END-IF-ELSE: On Event's Pixel's Nature

  } // ENDFOR each event

  if (dbprint) {
    g_log.information(dbss.str());
    g_log.notice() << "Number of signal counts = " << num_signal_block
                   << " in selected block."
                   << "\n";
  }

  // Update local statistics to their global counterparts
  PARALLEL_CRITICAL(ProcessVulcanFastLog_global_statistics) {
    this->num_good_events += local_num_good_events;
    this->num_ignored_events += local_num_ignored_events;
    this->num_error_events += local_num_error_events;

    this->num_bad_events += local_num_bad_events;
    this->num_wrongdetid_events += local_num_wrongdetid_events;

    std::set<PixelType>::iterator it;
    for (it = local_wrongdetids.begin(); it != local_wrongdetids.end(); ++it) {
      PixelType tmpid = *it;
      this->wrongdetids.insert(*it);

      // Create class map entry if not there
      size_t mindex = 0;
      auto git = this->wrongdetidmap.find(tmpid);
      if (git == this->wrongdetidmap.end()) {
        // create entry
        size_t newindex = this->wrongdetid_pulsetimes.size();
        this->wrongdetidmap[tmpid] = newindex;

        std::vector<Kernel::DateAndTime> temppulsetimes;
        std::vector<double> temptofs;
        this->wrongdetid_pulsetimes.push_back(temppulsetimes);
        this->wrongdetid_tofs.push_back(temptofs);

        mindex = newindex;
      } else {
        mindex = git->second;
      }

      // 2. Find local
      auto lit = local_pidindexmap.find(tmpid);
      size_t localindex = lit->second;

      for (size_t iv = 0; iv < local_pulsetimes[localindex].size(); iv++) {
        this->wrongdetid_pulsetimes[mindex].push_back(
            local_pulsetimes[localindex][iv]);
        this->wrongdetid_tofs[mindex].push_back(local_tofs[localindex][iv]);
      }
      // std::sort(this->wrongdetid_abstimes[mindex].begin(),
      // this->wrongdetid_abstimes[mindex].end());
    }

    if (local_shortest_tof < shortest_tof)
      shortest_tof = local_shortest_tof;
    if (local_longest_tof > longest_tof)
      longest_tof = local_longest_tof;
  } // END_CRITICAL
}

//----------------------------------------------------------------------------------------------
///** Comparator for sorting dasevent lists
//  */
// bool vzintermediatePixelIDComp(IntermediateEvent x, IntermediateEvent y) {
//  return (x.pid < y.pid);
//}

//-----------------------------------------------------------------------------
/**
  * Add a sample environment log for the proton chage (charge of the pulse in
  *picoCoulombs)
  * and set the scalar value (total proton charge, microAmps*hours, on the
  *sample)
  *
  * @param workspace :: Event workspace to set the proton charge on
  */
void ProcessVulcanFastLog::setProtonCharge(
    DataObjects::EventWorkspace_sptr &workspace) {
  if (this->proton_charge.empty()) // nothing to do
    return;

  Run &run = workspace->mutableRun();

  // Add the proton charge entries.
  TimeSeriesProperty<double> *log =
      new TimeSeriesProperty<double>("proton_charge");
  log->setUnits("picoCoulombs");

  // Add the time and associated charge to the log
  log->addValues(this->pulsetimes, this->proton_charge);

  /// TODO set the units for the log
  run.addLogData(log);
  // Force re-integration
  run.integrateProtonCharge();
  double integ = run.getProtonCharge();

  g_log.information() << "Total proton charge of " << integ
                      << " microAmp*hours found by integrating.\n";
}

//-----------------------------------------------------------------------------
/** Load a pixel mapping file
  * @param filename :: Path to file.
  */
void ProcessVulcanFastLog::loadPixelMap(const std::string &filename) {
  this->using_mapping_file = false;

  // check that there is a mapping file
  if (filename.empty()) {
    this->g_log.information("NOT using a mapping file");
    return;
  }

  // actually deal with the file
  this->g_log.debug("Using mapping file \"" + filename + "\"");

  // Open the file; will throw if there is any problem
  BinaryFile<PixelType> pixelmapFile(filename);
  PixelType max_pid = static_cast<PixelType>(pixelmapFile.getNumElements());
  // Load all the data
  this->pixelmap = pixelmapFile.loadAllIntoVector();

  // Check for funky file
  if (std::find_if(pixelmap.begin(), pixelmap.end(),
                   std::bind2nd(std::greater<PixelType>(), max_pid)) !=
      pixelmap.end()) {
    this->g_log.warning("Pixel id in mapping file was out of bounds. Loading "
                        "without mapping file");
    this->numpixel = 0;
    this->pixelmap.clear();
    this->using_mapping_file = false;
    return;
  }

  // If we got here, the mapping file was loaded correctly and we'll use it
  this->using_mapping_file = true;
  // Let's assume that the # of pixels in the instrument matches the mapping
  // file length.
  this->numpixel = static_cast<uint32_t>(pixelmapFile.getNumElements());
}

//-----------------------------------------------------------------------------
/** Open an event file
  * @param filename :: file to open.
  */
void ProcessVulcanFastLog::openEventFile(const std::string &filename) {
  // Open the file
  eventfile = new BinaryFile<DasEvent>(filename);
  num_events = eventfile->getNumElements();
  g_log.debug() << "File contains " << num_events << " event records.\n";

  // Check if we are only loading part of the event file
  const int chunk = getProperty("ChunkNumber");
  if (isEmpty(chunk)) // We are loading the whole file
  {
    first_event = 0;
    max_events = num_events;
  } else // We are loading part - work out the event number range
  {
    const int totalChunks = getProperty("TotalChunks");
    max_events = num_events / totalChunks;
    first_event = (chunk - 1) * max_events;
    // Need to add any remainder to the final chunk
    if (chunk == totalChunks)
      max_events += num_events % totalChunks;
  }

  g_log.information() << "Reading " << max_events << " event records\n";
}

//-----------------------------------------------------------------------------
/** Read a pulse ID file
 * @param filename :: file to load.
 * @param throwError :: Flag to trigger error throwing instead of just logging
 */
void ProcessVulcanFastLog::readPulseidFile(const std::string &filename,
                                           const bool throwError) {
  this->proton_charge_tot = 0.;
  this->num_pulses = 0;
  this->pulsetimesincreasing = true;

  // jump out early if there isn't a filename
  if (filename.empty()) {
    this->g_log.information("NOT using a pulseid file");
    return;
  }

  std::vector<Pulse> pulses;

  // set up for reading
  // Open the file; will throw if there is any problem
  try {
    BinaryFile<Pulse> pulseFile(filename);

    // Get the # of pulse
    this->num_pulses = pulseFile.getNumElements();
    this->g_log.information() << "Using pulseid file \"" << filename
                              << "\", with " << num_pulses << " pulses.\n";

    // Load all the data
    pulses = pulseFile.loadAll();
  } catch (runtime_error &e) {
    if (throwError) {
      throw;
    } else {
      this->g_log.information()
          << "Encountered error in pulseidfile (ignoring file): " << e.what()
          << "\n";
      return;
    }
  }

  if (num_pulses > 0) {
    double temp;
    DateAndTime lastPulseDateTime(0, 0);
    this->pulsetimes.reserve(num_pulses);
    for (const auto &pulse : pulses) {
      DateAndTime pulseDateTime(static_cast<int64_t>(pulse.seconds),
                                static_cast<int64_t>(pulse.nanoseconds));
      this->pulsetimes.push_back(pulseDateTime);
      this->event_indices.push_back(pulse.event_index);

      if (pulseDateTime < lastPulseDateTime)
        this->pulsetimesincreasing = false;
      else
        lastPulseDateTime = pulseDateTime;

      temp = pulse.pCurrent;
      this->proton_charge.push_back(temp);
      if (temp < 0.)
        this->g_log.warning("Individual proton charge < 0 being ignored");
      else
        this->proton_charge_tot += temp;
    }
  }

  this->proton_charge_tot = this->proton_charge_tot * CURRENT_CONVERSION;

  if (m_dbOpNumPulses > 0) {
    std::stringstream dbss;
    for (size_t i = 0; i < m_dbOpNumPulses; ++i)
      dbss << "[Pulse] " << i << "\t " << event_indices[i] << "\t "
           << pulsetimes[i].totalNanoseconds() << '\n';
    g_log.information(dbss.str());
  }
}

//----------------------------------------------------------------------------------------------
/** Process input properties for purpose of investigation
  */
void ProcessVulcanFastLog::processInvestigationInputs() {
  m_dbOpBlockNumber = getProperty("DBOutputBlockNumber");
  if (isEmpty(m_dbOpBlockNumber)) {
    m_dbOutput = false;
    m_dbOpBlockNumber = 0;
  } else {
    m_dbOutput = true;

    int numdbevents = getProperty("DBNumberOutputEvents");
    m_dbOpNumEvents = static_cast<size_t>(numdbevents);
  }

  int dbnumpulses = getProperty("DBNumberOutputPulses");
  if (!isEmpty(dbnumpulses))
    m_dbOpNumPulses = static_cast<size_t>(dbnumpulses);
  else
    m_dbOpNumPulses = 0;

  // about the investigation of sample logs
  m_logBlockNumber = getProperty("BlockNumberForLog");
  int outputLogRange = getProperty("EventRangeForLog");

  return;
}

//----------------------------------------------------------------------------------------------
void ProcessVulcanFastLog::createOutputs() {
  std::vector<double> vecx;
  std::vector<double> vecy;

  for (size_t iblock = 0; iblock < 100; ++iblock) {

    // create smaller workspace
    MatrixWorkspace_sptr blockws = WorkspaceFactory::Instance().create(
        "Workspace2D", 1, m_timeBlockVector[iblock].size(),
        m_valueBlockVector[iblock].size());

    // add entries
    for (size_t ientry = 0; ientry < m_timeBlockVector[iblock].size();
         ++ientry) {
      vecx.push_back(static_cast<double>(
          m_timeBlockVector[iblock][ientry].totalNanoseconds()));
      vecy.push_back(static_cast<double>(m_valueBlockVector[iblock][ientry]));

      blockws->mutableX(0)[ientry] = static_cast<double>(
          m_timeBlockVector[iblock][ientry].totalNanoseconds());
      blockws->mutableY(0)[ientry] =
          static_cast<double>(m_valueBlockVector[iblock][ientry]);
    }

    // add output
    std::stringstream wsnamess;
    wsnamess << "log_" << iblock;
    std::string wsname(wsnamess.str());

    AnalysisDataService::Instance().addOrReplace(wsname, blockws);
  }

  MatrixWorkspace_sptr logws = WorkspaceFactory::Instance().create(
      "Workspace2D", 1, vecx.size(), vecy.size());
  for (size_t i = 0; i < logws->histogram(0).size(); ++i) {
    logws->mutableX(0)[i] = vecx[i];
    logws->mutableY(0)[i] = vecy[i];
  }

  setProperty(OUT_PARAM, logws);
}

//----------------------------------------------------------------------------------------------
// TODO - Create 2nd outputs including the converted PIXEL ID and relative time in nano seconds
//        Workspaces are generated per N (input properties) blocks and output workspaces are
//        grouped
void blabla()
{

}

//----------------------------------------------------------------------------------------------
using namespace H5;

//----------------------------------------------------------------------------------------------
/**
 * @brief ProcessVulcanFastLog::exportTimeSeriesH5
 * @param h5name
 */
void ProcessVulcanFastLog::exportTimeSeriesH5_1D(
    const std::__cxx11::string &h5name) {

  // new and file
  H5::H5File *file = new H5::H5File(h5name, H5F_ACC_TRUNC);

  // create property list for a dataset and set up fill values
  int fillvalue = 0;
  H5::DSetCreatPropList plist;
  plist.setFillValue(H5::PredType::NATIVE_INT, &fillvalue);

  // create dataspace for the dataset in the file
  const int FSPACE_DIM1 =
      100; // Dimension sizes of the dataset as it is @ number of rows
  const int FSPACE_DIM2 = 10; //  stored in the file @ number of columns

H5:
  hsize_t fdim[] = {FSPACE_DIM1, FSPACE_DIM2}; // dim sizes of ds (on disk)

  const int FSPACE_RANK =
      2; // Dataset rank as it is stored in the file RANK2 = 2D

  // ***
  DataSpace fspace(FSPACE_RANK, fdim);

  // IS THIS PART NECESSARY?
  /*
   * Select hyperslab for the dataset in the file, using 3x2 blocks,
   * (4,3) stride and (2,4) count starting at the position (0,1).
   */
  hsize_t start[2];  // Start of hyperslab
  hsize_t stride[2]; // Stride of hyperslab
  hsize_t count[2];  // Block count
  hsize_t block[2];  // Block sizes
  start[0] = 0;
  start[1] = 0;
  stride[0] = 1;
  stride[1] = 3;
  count[0] = 16;
  count[1] = 1;
  block[0] = 1;
  block[1] = 3;
  fspace.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

  // Create dataspace for the first dataset.
  const int MSPACE_DIM1 = 8; // We will read dataset back from the file

  const int MSPACE1_RANK = 1; // Rank of the first dataset in memory
  const int MSPACE1_DIM = 50; // Dataset size in memory
  hsize_t dim1[] = {
      MSPACE1_DIM}; // Dimension size of the first dataset (in memory) */
  DataSpace mspace1(MSPACE1_RANK, dim1);

  const int MSPACE_RANK = 2; // Rank of the first dataset in memory

  // Select hyperslab.
  // We will use 48 elements of the vector buffer starting at the
  // second element.  Selected elements are 1 2 3 . . . 48
  start[0] = 0;
  stride[0] = 1;
  count[0] = 48;
  block[0] = 1;
  mspace1.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

  // write file
  int vector[MSPACE1_DIM]; // vector buffer for dset
  int matrix[2][2];
  /*
   * Buffer initialization.
   */
  vector[0] = vector[MSPACE1_DIM - 1] = -1;
  for (int i = 1; i < MSPACE1_DIM - 1; i++)
    vector[i] = i;

  // Create dataset and write it into the file.
  const H5std_string DATASET_NAME("Matrix in file");
  DataSet *dataset = new DataSet(
      file->createDataSet(DATASET_NAME, PredType::NATIVE_INT, fspace, plist));
  dataset->write(vector, PredType::NATIVE_INT, mspace1, fspace);

  // Reset the selection for the file dataspace fid.
  fspace.selectNone();

  // close file
  file->close();

  // Close the dataset and the file.
  delete dataset;
  delete file;

  return;
}

/** test for mapping from matrix to matrix
 */
void ProcessVulcanFastLog::exportTimeSeriesH5(
    const std::__cxx11::string &h5name, const int size) {

  // new and file
  H5::H5File *file = new H5::H5File(h5name, H5F_ACC_TRUNC);

  // create property list for a dataset and set up fill values
  int fillvalue = 0;
  H5::DSetCreatPropList plist;
  plist.setFillValue(H5::PredType::NATIVE_INT, &fillvalue);

  // create dataspace for the dataset in the file
  const int FSPACE_DIM1 =
      100; // Dimension sizes of the dataset as it is @ number of rows
  const int FSPACE_DIM2 = 10; //  stored in the file @ number of columns

H5:
  hsize_t fdim[] = {FSPACE_DIM1, FSPACE_DIM2}; // dim sizes of ds (on disk)

  const int FSPACE_RANK =
      2; // Dataset rank as it is stored in the file RANK2 = 2D

  // ***
  DataSpace fspace(FSPACE_RANK, fdim);

  // IS THIS PART NECESSARY?
  /*
   * Select hyperslab for the dataset in the file, using 3x2 blocks,
   * (4,3) stride and (2,4) count starting at the position (0,1).
   */
  hsize_t start[2];  // Start of hyperslab
  hsize_t stride[2]; // Stride of hyperslab
  hsize_t count[2];  // Block count
  hsize_t block[2];  // Block sizes
  start[0] = 0;
  start[1] = 0;
  stride[0] = 1;
  stride[1] = 3;
  count[0] = 16;
  count[1] = 1;
  block[0] = 1;
  block[1] = 3;
  fspace.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

  // Create dataspace for the first dataset.
  const int MSPACE_DIM1 = 8; // We will read dataset back from the file

  const int MSPACE1_RANK = 1; // Rank of the first dataset in memory
  const int MSPACE1_DIM = 50; // Dataset size in memory
  hsize_t dim1[] = {
      MSPACE1_DIM}; // Dimension size of the first dataset (in memory) */
  DataSpace mspace1(MSPACE1_RANK, dim1);

  const int MSPACE_RANK = 2; // Rank of the first dataset in memory

  // Select hyperslab.
  // We will use 48 elements of the vector buffer starting at the
  // second element.  Selected elements are 1 2 3 . . . 48
  start[0] = 0;
  stride[0] = 1;
  count[0] = 48;
  block[0] = 1;
  mspace1.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

  // write file
  // int vector[MSPACE1_DIM]; // vector buffer for dset
  int *vector = new int[size];

  /*
   * Buffer initialization.
   */
  vector[0] = vector[MSPACE1_DIM - 1] = -1;
  for (int i = 1; i < MSPACE1_DIM - 1; i++)
    vector[i] = i;

  // Create dataset and write it into the file.
  const H5std_string DATASET_NAME("Matrix in file");
  DataSet *dataset = new DataSet(
      file->createDataSet(DATASET_NAME, PredType::NATIVE_INT, fspace, plist));
  dataset->write(vector, PredType::NATIVE_INT, mspace1, fspace);

  // Reset the selection for the file dataspace fid.
  fspace.selectNone();

  // close file
  file->close();

  // Close the dataset and the file.
  delete dataset;
  delete file;

  return;
}

//---
void ProcessVulcanFastLog::exportTimeSeriesH5_raw(const std::string &h5name) {
  // new and file
  H5::H5File *file = new H5::H5File(h5name, H5F_ACC_TRUNC);

  // create property list for a dataset and set up fill values
  int fillvalue = 0;
  H5::DSetCreatPropList plist;
  plist.setFillValue(H5::PredType::NATIVE_INT, &fillvalue);

  // create dataspace for the dataset in the file
  const int FSPACE_DIM1 = 10; // Dimension sizes of the dataset as it is
  const int FSPACE_DIM2 = 12; //  stored in the file

H5:
  hsize_t fdim[] = {FSPACE_DIM1, FSPACE_DIM2}; // dim sizes of ds (on disk)

  const int FSPACE_RANK = 2; // Dataset rank as it is stored in the file
  DataSpace fspace(FSPACE_RANK, fdim);

  // IS THIS PART NECESSARY?
  /*
   * Select hyperslab for the dataset in the file, using 3x2 blocks,
   * (4,3) stride and (2,4) count starting at the position (0,1).
   */
  hsize_t start[2];  // Start of hyperslab
  hsize_t stride[2]; // Stride of hyperslab
  hsize_t count[2];  // Block count
  hsize_t block[2];  // Block sizes
                     //    start[0]  = 0; start[1]  = 0;
                     //    stride[0] = 4; stride[1] = 3;
                     //    count[0]  = 2; count[1]  = 4;
                     //    block[0]  = 3; block[1]  = 2;

  start[0] = 0;
  start[1] = 0;
  stride[0] = 3;
  stride[1] = 2;
  count[0] = 2;
  count[1] = 6;
  block[0] = 3;
  block[1] = 2;

  fspace.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

  // file will be stride * block = 3, all filled

  // Create dataspace for the first dataset.
  const int MSPACE_DIM1 = 8; // We will read dataset back from the file

  const int MSPACE1_RANK = 1; // Rank of the first dataset in memory
  const int MSPACE1_DIM = 72; // Dataset size in memory
  hsize_t dim1[] = {
      MSPACE1_DIM}; // Dimension size of the first dataset (in memory) */
  DataSpace mspace1(MSPACE1_RANK, dim1);

  const int MSPACE_RANK = 2; // Rank of the first dataset in memory

  // Select hyperslab.
  // We will use 48 elements of the vector buffer starting at the
  // second element.  Selected elements are 1 2 3 . . . 48
  start[0] = 0;  // start from 0
  stride[0] = 1; // stride[1] = 1;  // separate 1 from vertical (contiuous),
                 // separate 2 from horizontal (continous)
  count[0] = 72; // total number to write
  block[0] = 1;  // block size 1, 2
  mspace1.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

  // write file
  int vector[MSPACE1_DIM]; // vector buffer for dset
                           /*
                            * Buffer initialization.
                            */
  vector[0] = vector[MSPACE1_DIM - 1] = -5;
  for (int i = 1; i < MSPACE1_DIM - 1; i++)
    vector[i] = i;

  // Create dataset and write it into the file.
  const H5std_string DATASET_NAME("Matrix in file");
  DataSet *dataset = new DataSet(
      file->createDataSet(DATASET_NAME, PredType::NATIVE_INT, fspace, plist));
  dataset->write(vector, PredType::NATIVE_INT, mspace1, fspace);

  // Reset the selection for the file dataspace fid.
  fspace.selectNone();

  // close file
  file->close();

  // Close the dataset and the file.
  delete dataset;
  delete file;

  return;
}

} // namespace DataHandling
} // namespace Mantid