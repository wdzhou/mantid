#include "MantidAlgorithms/DetectorEfficiencyCorUser.h"
#include "MantidAPI/HistogramValidator.h"
#include "MantidAPI/InstrumentValidator.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidAPI/WorkspaceUnitValidator.h"
#include "MantidGeometry/Instrument.h"
#include "MantidGeometry/muParser_Silent.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/CompositeValidator.h"

namespace Mantid {
namespace Algorithms {

using namespace Kernel;
using namespace API;
using namespace Geometry;

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(DetectorEfficiencyCorUser)

//----------------------------------------------------------------------------------------------
/// Algorithm's name for identification. @see Algorithm::name
const std::string DetectorEfficiencyCorUser::name() const {
  return "DetectorEfficiencyCorUser";
}

/// Algorithm's version for identification. @see Algorithm::version
int DetectorEfficiencyCorUser::version() const { return 1; }

/// Algorithm's category for identification. @see Algorithm::category
const std::string DetectorEfficiencyCorUser::category() const {
  return "CorrectionFunctions\\EfficiencyCorrections;Inelastic\\Corrections";
}

//----------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm's properties.
 */
void DetectorEfficiencyCorUser::init() {
  auto val = boost::make_shared<CompositeValidator>();
  // val->add<WorkspaceUnitValidator>("Energy");
  val->add<WorkspaceUnitValidator>("DeltaE");
  val->add<HistogramValidator>();
  val->add<InstrumentValidator>();
  declareProperty(make_unique<WorkspaceProperty<>>("InputWorkspace", "",
                                                   Direction::Input, val),
                  "The workspace to correct for detector efficiency");
  declareProperty(make_unique<WorkspaceProperty<>>("OutputWorkspace", "",
                                                   Direction::Output),
                  "The name of the workspace in which to store the result.");
  auto checkEi = boost::make_shared<BoundedValidator<double>>();
  checkEi->setLower(0.0);
  declareProperty("IncidentEnergy", EMPTY_DBL(), checkEi,
                  "The energy of neutrons leaving the source.");
}

//----------------------------------------------------------------------------------------------
/** Execute the algorithm.
 */
void DetectorEfficiencyCorUser::exec() {

  // get input properties (WSs, Ei)
  retrieveProperties();

  // get Efficiency formula from the IDF
  const std::string effFormula = getValFromInstrumentDef("formula_eff");

  // Calculate Efficiency for E = Ei
  const double eff0 = calculateFormulaValue(effFormula, m_Ei);

  const size_t numberOfChannels = this->m_inputWS->blocksize();
  // Calculate the number of spectra in this workspace
  const int numberOfSpectra =
      static_cast<int>(this->m_inputWS->size() / numberOfChannels);
  API::Progress prog(this, 0.0, 1.0, numberOfSpectra);
  int64_t numberOfSpectra_i =
      static_cast<int64_t>(numberOfSpectra); // cast to make openmp happy

  // Loop over the histograms (detector spectra)
  PARALLEL_FOR2(m_outputWS, m_inputWS)
  for (int64_t i = 0; i < numberOfSpectra_i; ++i) {
    PARALLEL_START_INTERUPT_REGION

    // MantidVec& xOut = m_outputWS->dataX(i);
    auto &yOut = m_outputWS->mutableY(i);
    auto &eOut = m_outputWS->mutableE(i);
    auto &xIn = m_inputWS->x(i);
    auto &yIn = m_inputWS->y(i);
    auto &eIn = m_inputWS->e(i);
    m_outputWS->setSharedX(i, m_inputWS->sharedX(i));

    const MantidVec effVec = calculateEfficiency(eff0, effFormula, xIn);
    // run this outside to benefit from parallel for (?)
    applyDetEfficiency(numberOfChannels, yIn, eIn, effVec, yOut, eOut);

    prog.report("Detector Efficiency correction...");

    PARALLEL_END_INTERUPT_REGION
  } // end for i
  PARALLEL_CHECK_INTERUPT_REGION

  this->setProperty("OutputWorkspace", this->m_outputWS);
}

/**
 * Apply the detector efficiency to a single spectrum
 * @param numberOfChannels Number of channels in a spectra (nbins - 1)
 * @param yIn spectrum counts
 * @param eIn spectrum errors
 * @param effVec efficiency values (to be divided by the counts)
 * @param yOut corrected spectrum counts
 * @param eOut corrected spectrum errors
 */
void DetectorEfficiencyCorUser::applyDetEfficiency(
    const size_t numberOfChannels, const Mantid::HistogramData::HistogramY &yIn,
    const Mantid::HistogramData::HistogramE &eIn, const MantidVec &effVec,
    Mantid::HistogramData::HistogramY &yOut,
    Mantid::HistogramData::HistogramE &eOut) {

  for (unsigned int j = 0; j < numberOfChannels; ++j) {
    // xOut[j] = xIn[j];
    yOut[j] = yIn[j] / effVec[j];
    eOut[j] = eIn[j] / effVec[j];
  }
}
/**
 * Calculate the value of a formula
 * @param formula :: Formula
 * @param energy :: value to use in the formula
 * @return value calculated
 */
double
DetectorEfficiencyCorUser::calculateFormulaValue(const std::string &formula,
                                                 double energy) {
  try {
    mu::Parser p;
    p.DefineVar("e", &energy);
    p.SetExpr(formula);
    double eff = p.Eval();
    g_log.debug() << "Formula: " << formula << " with: " << energy
                  << "evaluated to: " << eff << '\n';
    return eff;

  } catch (mu::Parser::exception_type &e) {
    throw Kernel::Exception::InstrumentDefinitionError(
        "Error calculating formula from string. Muparser error message is: " +
        e.GetMsg());
  }
}

/**
 * Calculate detector efficiency given a formula, the efficiency at the elastic
 * line,
 * and a vector with energies.
 *  Efficiency = f(Ei-DeltaE) / f(Ei)
 * Hope all compilers supports the NRVO (otherwise will copy the output vector)
 * @param eff0 :: calculated eff0
 * @param formula :: formula to calculate efficiency (parsed from IDF)
 * @param xIn :: Energy bins vector (X axis)
 * @return a vector with the efficiencies
 */
MantidVec DetectorEfficiencyCorUser::calculateEfficiency(
    double eff0, const std::string &formula,
    const Mantid::HistogramData::HistogramX &xIn) {

  MantidVec effOut(xIn.size() - 1); // x are bins and have more one value than y

  try {
    double e;
    mu::Parser p;
    p.DefineVar("e", &e);
    p.SetExpr(formula);

    // copied from Jaques Ollivier Code
    bool conditionForEnergy =
        std::min(std::abs(*std::min_element(xIn.begin(), xIn.end())), m_Ei) <
        m_Ei;

    auto xIn_it = xIn.cbegin(); // DeltaE
    auto effOut_it = effOut.begin();
    for (; effOut_it != effOut.end(); ++xIn_it, ++effOut_it) {
      if (conditionForEnergy) {
        // cppcheck cannot see that this is used by reference by muparser
        e = std::fabs(m_Ei + *xIn_it);
      } else {
        // cppcheck cannot see that this is used by reference by muparser
        // cppcheck-suppress unreadVariable
        e = std::fabs(m_Ei - *xIn_it);
      }
      double eff = p.Eval();
      *effOut_it = eff / eff0;
    }
    return effOut;
  } catch (mu::Parser::exception_type &e) {
    throw Kernel::Exception::InstrumentDefinitionError(
        "Error calculating formula from string. Muparser error message is: " +
        e.GetMsg());
  }
}
/**
 * Returns the value associated to a parameter name in the IDF
 * @param parameterName :: parameter name in the IDF
 * @return the value associated to the parameter name
 */
std::string DetectorEfficiencyCorUser::getValFromInstrumentDef(
    const std::string &parameterName) {

  const ParameterMap &pmap = m_inputWS->constInstrumentParameters();
  Instrument_const_sptr instrument = m_inputWS->getInstrument();
  Parameter_sptr par =
      pmap.getRecursive(instrument->getChild(0).get(), parameterName);
  if (par) {
    std::string ret = par->asString();
    g_log.debug() << "Parsed parameter " << parameterName << ": " << ret
                  << "\n";
    return ret;
  } else {
    throw Kernel::Exception::InstrumentDefinitionError(
        "There is no <" + parameterName + "> in the instrument definition!");
  }
}

/** Loads and checks the values passed to the algorithm
 *
 *  @throw invalid_argument if there is an incapatible property value so the
 *algorithm can't continue
 */
void DetectorEfficiencyCorUser::retrieveProperties() {

  // Get the workspaces
  m_inputWS = this->getProperty("InputWorkspace");

  m_outputWS = this->getProperty("OutputWorkspace");

  // If input and output workspaces are not the same, create a new workspace for
  // the output
  if (m_outputWS != this->m_inputWS) {
    m_outputWS = API::WorkspaceFactory::Instance().create(m_inputWS);
  }

  // these first three properties are fully checked by validators
  m_Ei = this->getProperty("IncidentEnergy");
  // If we're not given an Ei, see if one has been set.
  if (m_Ei == EMPTY_DBL()) {
    Mantid::Kernel::Property *prop = m_inputWS->run().getProperty("Ei");
    double val;
    if (!prop || !Strings::convert(prop->value(), val)) {
      throw std::invalid_argument(
          "No Ei value has been set or stored within the run information.");
    }
    m_Ei = val;
    g_log.debug() << "Using stored Ei value " << m_Ei << "\n";
  } else {
    g_log.debug() << "Using user input Ei value: " << m_Ei << "\n";
  }
}
}
// namespace Algorithms
} // namespace Mantid
