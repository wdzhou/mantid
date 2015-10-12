#include "MantidMDAlgorithms/ConvertCWSDMDtoHKL.h"

#include "MantidAPI/WorkspaceProperty.h"
#include "MantidAPI/IMDEventWorkspace.h"
#include "MantidAPI/IMDIterator.h"

#include "MantidGeometry/Crystal/IndexingUtils.h"
#include "MantidGeometry/Crystal/OrientedLattice.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/ArrayProperty.h"

#include "MantidDataObjects/MDEventFactory.h"
#include "MantidAPI/ExperimentInfo.h"
#include "MantidGeometry/Instrument/ComponentHelper.h"
#include "MantidMDAlgorithms/MDWSDescription.h"
#include "MantidMDAlgorithms/MDWSTransform.h"
#include "MantidAPI/FileProperty.h"
#include "MantidDataObjects/MDBoxBase.h"
#include "MantidDataObjects/MDEventInserter.h"

#include <fstream>

namespace Mantid {
namespace MDAlgorithms {

using namespace Mantid::API;
using namespace Mantid::Kernel;
using namespace Mantid::MDAlgorithms;
using namespace Mantid::DataObjects;
using namespace Mantid::Geometry;

DECLARE_ALGORITHM(ConvertCWSDMDtoHKL)

//----------------------------------------------------------------------------------------------
/** Constructor
 */
ConvertCWSDMDtoHKL::ConvertCWSDMDtoHKL() {}

//----------------------------------------------------------------------------------------------
/** Destructor
 */
ConvertCWSDMDtoHKL::~ConvertCWSDMDtoHKL() {}

//----------------------------------------------------------------------------------------------
/** Init
 */
void ConvertCWSDMDtoHKL::init() {
  declareProperty(new WorkspaceProperty<IMDEventWorkspace>("InputWorkspace", "",
                                                           Direction::Input),
                  "Name of the input MDEventWorkspace that stores detectors "
                  "counts from a constant-wave powder diffraction experiment.");

  declareProperty(new WorkspaceProperty<PeaksWorkspace>("PeaksWorkspace", "",
                                                        Direction::Input,
                                                        PropertyMode::Optional),
                  "Input Peaks Workspace");

  declareProperty(
      new ArrayProperty<double>("UBMatrix"),
      "A comma seperated list of doubles for UB matrix from (0,0), (0,1)"
      "... (2,1),(2,2)");

  declareProperty(new WorkspaceProperty<IMDEventWorkspace>(
                      "OutputWorkspace", "", Direction::Output),
                  "Name of the output MDEventWorkspace in HKL-space.");

  std::vector<std::string> exts;
  exts.push_back(".dat");
  declareProperty(
      new FileProperty("QSampleFileName", "", API::FileProperty::OptionalSave),
      "Name of file for sample sample.");

  declareProperty(
      new FileProperty("HKLFileName", "", API::FileProperty::OptionalSave),
      "Name of file for HKL.");
}

//----------------------------------------------------------------------------------------------
/** Exec
 */
void ConvertCWSDMDtoHKL::exec() {
  // Get inputs
  IMDEventWorkspace_sptr inputWS = getProperty("InputWorkspace");
  if (inputWS->getSpecialCoordinateSystem() != Mantid::Kernel::QSample) {
    std::stringstream errmsg;
    errmsg << "Input MDEventWorkspace's coordinate system is not QSample but "
           << inputWS->getSpecialCoordinateSystem() << ".";
    throw std::invalid_argument(errmsg.str());
  }

  getUBMatrix();

  // Test indexing.  Will be delete soon
  if (true) {
    Kernel::V3D qsample; // [1.36639,-2.52888,-4.77349]
    qsample.setX(1.36639);
    qsample.setY(-2.52888);
    qsample.setZ(-4.77349);

    std::vector<Kernel::V3D> vec_qsample;
    vec_qsample.push_back(qsample);
    std::vector<Kernel::V3D> vec_mindex(0);

    convertFromQSampleToHKL(vec_qsample, vec_mindex);
    g_log.notice() << "[DB Number of returned Miller Index = "
                   << vec_mindex.size() << "\n";
    g_log.notice() << "[DB] Output HKL = " << vec_mindex[0].toString() << "\n";
  }

  // Get events information for future processing
  std::vector<Kernel::V3D> vec_event_qsample;
  std::vector<signal_t> vec_event_signal;
  std::vector<detid_t> vec_event_det;
  exportEvents(inputWS, vec_event_qsample, vec_event_signal, vec_event_det);

  std::string qsamplefilename = getPropertyValue("QSampleFileName");
  // Abort with an empty string
  if (qsamplefilename.size() > 0)
    saveEventsToFile(qsamplefilename, vec_event_qsample, vec_event_signal,
                     vec_event_det);

  // Convert to HKL
  std::vector<Kernel::V3D> vec_event_hkl;
  convertFromQSampleToHKL(vec_event_qsample, vec_event_hkl);

  // Get file name
  std::string hklfilename = getPropertyValue("HKLFileName");
  // Abort mission if no file name is given
  if (hklfilename.size() > 0)
    saveEventsToFile(hklfilename, vec_event_hkl, vec_event_signal,
                     vec_event_det);

  // Create output workspace
  m_outputWS =
      createHKLMDWorkspace(vec_event_hkl, vec_event_signal, vec_event_det);

  setProperty("OutputWorkspace", m_outputWS);
}

void ConvertCWSDMDtoHKL::getUBMatrix() {
  std::string peakwsname = getPropertyValue("PeaksWorkspace");
  if (peakwsname.size() > 0 &&
      AnalysisDataService::Instance().doesExist(peakwsname)) {
    // Get from peak works
    DataObjects::PeaksWorkspace_sptr peakws = getProperty("PeaksWorkspace");
    m_UB =
        Kernel::Matrix<double>(peakws->sample().getOrientedLattice().getUB());
  } else {
    // Get from ...
    std::vector<double> ub_array = getProperty("UBMatrix");
    if (ub_array.size() != 9)
      throw std::invalid_argument("Input UB matrix must have 9 elements");

    m_UB = Kernel::Matrix<double>(3, 3);
    for (size_t i = 0; i < 3; ++i) {
      for (size_t j = 0; j < 3; ++j) {
        m_UB[i][j] = ub_array[i * 3 + j];
      }
    }
  }

  return;
}

//--------------------------------------------------------------------------
/** Export events from an MDEventWorkspace for future processing
 * It is a convenient algorithm if number of events are few relative to
 * number of detectors
 */
void ConvertCWSDMDtoHKL::exportEvents(
    IMDEventWorkspace_sptr mdws, std::vector<Kernel::V3D> &vec_event_qsample,
    std::vector<signal_t> &vec_event_signal,
    std::vector<detid_t> &vec_event_det) {
  // Set the size of the output vectors
  size_t numevents = mdws->getNEvents();
  g_log.information() << "Number of events = " << numevents << "\n";

  vec_event_qsample.resize(numevents);
  vec_event_signal.resize(numevents);
  vec_event_det.resize(numevents);

  // Go through to get value
  IMDIterator *mditer = mdws->createIterator();
  size_t nextindex = 1;
  bool scancell = true;
  size_t currindex = 0;
  while (scancell) {
    size_t numevent_cell = mditer->getNumEvents();
    for (size_t iev = 0; iev < numevent_cell; ++iev) {
      // Check
      if (currindex >= vec_event_qsample.size())
        throw std::runtime_error("Logic error in event size!");

      float tempx = mditer->getInnerPosition(iev, 0);
      float tempy = mditer->getInnerPosition(iev, 1);
      float tempz = mditer->getInnerPosition(iev, 2);
      signal_t signal = mditer->getInnerSignal(iev);
      detid_t detid = mditer->getInnerDetectorID(iev);
      Kernel::V3D qsample(tempx, tempy, tempz);
      vec_event_qsample[currindex] = qsample;
      vec_event_signal[currindex] = signal;
      vec_event_det[currindex] = detid;

      ++currindex;
    }

    // Advance to next cell
    if (mditer->next()) {
      // advance to next cell
      mditer->jumpTo(nextindex);
      ++nextindex;
    } else {
      // break the loop
      scancell = false;
    }
  }

  return;
}

//--------------------------------------------------------------------------
/** Save Q-sample to file
 */
void ConvertCWSDMDtoHKL::saveMDToFile(
    std::vector<std::vector<coord_t>> &vec_event_qsample,
    std::vector<float> &vec_event_signal) {
  // Get file name
  std::string filename = getPropertyValue("QSampleFileName");

  // Abort with an empty string
  if (filename.size() == 0)
    return;
  if (vec_event_qsample.size() != vec_event_signal.size())
    throw std::runtime_error(
        "Input vectors of Q-sample and signal have different sizes.");

  // Write to file
  std::ofstream ofile;
  ofile.open(filename.c_str());

  size_t numevents = vec_event_qsample.size();
  for (size_t i = 0; i < numevents; ++i) {
    ofile << vec_event_qsample[i][0] << ", " << vec_event_qsample[i][1] << ", "
          << vec_event_qsample[i][2] << ", " << vec_event_signal[i] << "\n";
  }
  ofile.close();

  return;
}

//--------------------------------------------------------------------------
/** Save HKL to file for 3D visualization
 */
void ConvertCWSDMDtoHKL::saveEventsToFile(
    const std::string &filename, std::vector<Kernel::V3D> &vec_event_pos,
    std::vector<signal_t> &vec_event_signal,
    std::vector<detid_t> &vec_event_detid) {
  // Check
  if (vec_event_detid.size() != vec_event_pos.size() ||
      vec_event_pos.size() != vec_event_signal.size())
    throw std::invalid_argument(
        "Input vectors for HKL, signal and detector ID have different size.");

  std::ofstream ofile;
  ofile.open(filename.c_str());

  size_t numevents = vec_event_pos.size();
  for (size_t i = 0; i < numevents; ++i) {
    ofile << vec_event_pos[i].X() << ", " << vec_event_pos[i].Y() << ", "
          << vec_event_pos[i].Z() << ", " << vec_event_signal[i] << ", "
          << vec_event_detid[i] << "\n";
  }
  ofile.close();

  return;
}

//--------------------------------------------------------------------------
/** Convert from Q-sample to HKL
 */
void ConvertCWSDMDtoHKL::convertFromQSampleToHKL(
    const std::vector<V3D> &q_vectors, std::vector<V3D> &miller_indices) {

  Matrix<double> tempUB(m_UB);

  int original_indexed = 0;
  double original_error = 0;
  double tolerance = 0.55; // to make sure no output is invalid
  original_indexed = IndexingUtils::CalculateMillerIndices(
      tempUB, q_vectors, tolerance, miller_indices, original_error);

  g_log.notice() << "[DB] " << original_indexed << " peaks are indexed."
                 << "\n";

  return;
}

//----------------------------------------------------------------------------------------------
/** Create output workspace
 * @brief ConvertCWSDExpToMomentum::createExperimentMDWorkspace
 * @return
 */
API::IMDEventWorkspace_sptr ConvertCWSDMDtoHKL::createHKLMDWorkspace(
    const std::vector<Kernel::V3D> &vec_hkl,
    const std::vector<signal_t> &vec_signal,
    const std::vector<detid_t> &vec_detid) {
  // Check
  if (vec_hkl.size() != vec_signal.size() ||
      vec_signal.size() != vec_detid.size())
    throw std::invalid_argument("Input vectors for HKL, signal and detector "
                                "IDs are of different size!");

  // Create workspace in Q_sample with dimenion as 3
  size_t nDimension = 3;
  IMDEventWorkspace_sptr mdws =
      MDEventFactory::CreateMDWorkspace(nDimension, "MDEvent");

  // Extract Dimensions and add to the output workspace.
  std::vector<std::string> vec_ID(3);
  vec_ID[0] = "H";
  vec_ID[1] = "K";
  vec_ID[2] = "L";

  std::vector<std::string> dimensionNames(3);
  dimensionNames[0] = "H";
  dimensionNames[1] = "K";
  dimensionNames[2] = "L";

  Mantid::Kernel::SpecialCoordinateSystem coordinateSystem =
      Mantid::Kernel::HKL;

  // Add dimensions
  std::vector<double> m_extentMins(3);
  std::vector<double> m_extentMaxs(3);
  std::vector<size_t> m_numBins(3, 100);
  getRange(vec_hkl, m_extentMins, m_extentMaxs);

  for (size_t i = 0; i < nDimension; ++i) {
    std::string id = vec_ID[i];
    std::string name = dimensionNames[i];
    // std::string units = "A^-1";
    std::string units = "";
    mdws->addDimension(
        Geometry::MDHistoDimension_sptr(new Geometry::MDHistoDimension(
            id, name, units, static_cast<coord_t>(m_extentMins[i]),
            static_cast<coord_t>(m_extentMaxs[i]), m_numBins[i])));
  }

  // Set coordinate system
  mdws->setCoordinateSystem(coordinateSystem);

  // Creates a new instance of the MDEventInserter to output workspace
  MDEventWorkspace<MDEvent<3>, 3>::sptr mdws_mdevt_3 =
      boost::dynamic_pointer_cast<MDEventWorkspace<MDEvent<3>, 3>>(mdws);
  MDEventInserter<MDEventWorkspace<MDEvent<3>, 3>::sptr> inserter(mdws_mdevt_3);

  // Go though each spectrum to conver to MDEvent
  for (size_t iq = 0; iq < vec_hkl.size(); ++iq) {
    Kernel::V3D hkl = vec_hkl[iq];
    std::vector<Mantid::coord_t> millerindex(3);
    millerindex[0] = static_cast<float>(hkl.X());
    millerindex[1] = static_cast<float>(hkl.Y());
    millerindex[2] = static_cast<float>(hkl.Z());

    signal_t signal = vec_signal[iq];
    signal_t error = std::sqrt(signal);
    uint16_t runnumber = 1;
    detid_t detid = vec_detid[iq];

    // Insert
    inserter.insertMDEvent(
        static_cast<float>(signal), static_cast<float>(error * error),
        static_cast<uint16_t>(runnumber), detid, millerindex.data());
  }

  return mdws;
}

void ConvertCWSDMDtoHKL::getRange(const std::vector<Kernel::V3D> vec_hkl,
                                  std::vector<double> &extentMins,
                                  std::vector<double> &extentMaxs) {
  assert(extentMins.size() == 3);
  assert(extentMaxs.size() == 3);

  for (size_t i = 0; i < 3; ++i) {
    double minvalue = vec_hkl[0][i];
    double maxvalue = vec_hkl[0][i];
    for (size_t j = 1; j < vec_hkl.size(); ++j) {
      double thisvalue = vec_hkl[j][i];
      if (thisvalue < minvalue)
        minvalue = thisvalue;
      else if (thisvalue > maxvalue)
        maxvalue = thisvalue;
    }
    extentMins[i] = minvalue;
    extentMaxs[i] = maxvalue;
  }

  return;
}

} // namespace MDAlgorithms
} // namespace Mantid