//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidAlgorithms/FFTSmooth.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAPI/TextAxis.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidKernel/Exception.h"

#include "MantidKernel/BoundedValidator.h"
#include "MantidKernel/ListValidator.h"

namespace Mantid {
namespace Algorithms {

// Register the class into the algorithm factory
DECLARE_ALGORITHM(FFTSmooth)

using namespace Kernel;
using namespace API;

/// Initialisation method. Declares properties to be used in algorithm.
void FFTSmooth::init() {
  declareProperty(make_unique<WorkspaceProperty<API::MatrixWorkspace>>(
                      "InputWorkspace", "", Direction::Input),
                  "The name of the input workspace.");
  declareProperty(make_unique<WorkspaceProperty<API::MatrixWorkspace>>(
                      "OutputWorkspace", "", Direction::Output),
                  "The name of the output workspace.");

  auto mustBePositive = boost::make_shared<BoundedValidator<int>>();
  mustBePositive->setLower(0);
  declareProperty("WorkspaceIndex", 0, mustBePositive,
                  "Workspace index for smoothing");

  std::vector<std::string> type{"Zeroing"};
  declareProperty("Filter", "Zeroing",
                  boost::make_shared<StringListValidator>(type),
                  "The type of the applied filter");
  declareProperty("Params", "", "The filter parameters");
}

/** Executes the algorithm
 */
void FFTSmooth::exec() {
  m_inWS = getProperty("InputWorkspace");
  int spec = getProperty("WorkspaceIndex");

  // Save the starting x value so it can be restored after all transforms.
  double x0 = m_inWS->x(spec)[0];

  // Symmetrize the input spectrum
  int dn = static_cast<int>(m_inWS->y(0).size());

  API::MatrixWorkspace_sptr symmWS = API::WorkspaceFactory::Instance().create(
      "Workspace2D", 1, m_inWS->x(0).size() + dn, m_inWS->y(0).size() + dn);

  double dx = (m_inWS->x(spec).back() - m_inWS->x(spec).front()) /
              static_cast<double>(m_inWS->x(spec).size() - 1);

  auto &symX = symmWS->mutableX(0);
  auto &symY = symmWS->mutableY(0);

  for (int i = 0; i < dn; i++) {
    symX[dn + i] = m_inWS->x(spec)[i];
    symY[dn + i] = m_inWS->y(spec)[i];

    symX[dn - i] = x0 - dx * i;
    symY[dn - i] = m_inWS->y(spec)[i];
  }
  symmWS->mutableY(0).front() = m_inWS->y(spec).back();
  symmWS->mutableX(0).front() = x0 - dx * dn;
  if (m_inWS->isHistogramData())
    symmWS->mutableX(0).back() = m_inWS->x(spec).back();

  // Forward Fourier transform
  IAlgorithm_sptr fft = createChildAlgorithm("RealFFT", 0, 0.5);
  fft->setProperty("InputWorkspace", symmWS);
  fft->setProperty("WorkspaceIndex", 0);
  try {
    fft->execute();
  } catch (...) {
    g_log.error("Error in direct FFT algorithm");
    throw;
  }

  m_unfilteredWS = fft->getProperty("OutputWorkspace");

  // Apply the filter
  std::string type = getProperty("Filter");

  if (type == "Zeroing") {
    std::string sn = getProperty("Params");
    int n;
    if (sn.empty())
      n = 2;
    else
      n = atoi(sn.c_str());
    if (n < 1)
      throw std::invalid_argument(
          "Truncation parameter must be an integer > 1");
    zero(n);
  }

  // Backward transform
  fft = createChildAlgorithm("RealFFT", 0.5, 1.);
  fft->setProperty("InputWorkspace", m_filteredWS);
  fft->setProperty("Transform", "Backward");
  try {
    fft->execute();
  } catch (...) {
    g_log.error("Error in inverse FFT algorithm");
    throw;
  }
  API::MatrixWorkspace_sptr tmpWS = fft->getProperty("OutputWorkspace");

  // Create output
  API::MatrixWorkspace_sptr outWS = API::WorkspaceFactory::Instance().create(
      m_inWS, 1, m_inWS->x(0).size(), m_inWS->y(0).size());

  dn = static_cast<int>(tmpWS->blocksize()) / 2;

  outWS->mutableX(0).assign(tmpWS->x(0).cbegin() + dn, tmpWS->x(0).cend());
  outWS->mutableY(0).assign(tmpWS->y(0).cbegin() + dn, tmpWS->y(0).cend());

  setProperty("OutputWorkspace", outWS);
}

/** Smoothing by truncation.
 *  @param n :: The order of truncation
 */
void FFTSmooth::truncate(int n) {
  int my = static_cast<int>(m_unfilteredWS->y(0).size());
  int ny = my / n;

  double f = double(ny) / my;

  if (ny == 0)
    ny = 1;
  int nx = m_unfilteredWS->isHistogramData() ? ny + 1 : ny;
  m_filteredWS =
      API::WorkspaceFactory::Instance().create(m_unfilteredWS, 2, nx, ny);

  auto &Yr = m_unfilteredWS->y(0);
  auto &Yi = m_unfilteredWS->y(1);
  auto &X = m_unfilteredWS->x(0);

  auto &yr = m_filteredWS->mutableY(0);
  auto &yi = m_filteredWS->mutableY(1);
  auto &xr = m_filteredWS->mutableX(0);
  auto &xi = m_filteredWS->mutableX(1);

  yr.assign(Yr.begin(), Yr.begin() + ny);
  yi.assign(Yi.begin(), Yi.begin() + ny);
  xr.assign(X.begin(), X.begin() + nx);
  xi.assign(X.begin(), X.begin() + nx);

  std::transform(yr.begin(), yr.end(), yr.begin(),
                 std::bind2nd(std::multiplies<double>(), f));
  std::transform(yi.begin(), yi.end(), yi.begin(),
                 std::bind2nd(std::multiplies<double>(), f));
}

/** Smoothing by zeroing.
 *  @param n :: The order of truncation
 */
void FFTSmooth::zero(int n) {
  int mx = static_cast<int>(m_unfilteredWS->x(0).size());
  int my = static_cast<int>(m_unfilteredWS->y(0).size());
  int ny = my / n;

  if (ny == 0)
    ny = 1;

  m_filteredWS =
      API::WorkspaceFactory::Instance().create(m_unfilteredWS, 2, mx, my);
  
  m_filteredWS->setSharedX(0, m_unfilteredWS->sharedX(0));
  m_filteredWS->setSharedX(1, m_unfilteredWS->sharedX(0));

  std::copy(m_unfilteredWS->y(0).cbegin(), m_unfilteredWS->y(0).begin() + ny,
	  m_filteredWS->mutableY(0).begin());

  std::copy(m_unfilteredWS->y(1).cbegin(), m_unfilteredWS->y(1).begin() + ny,
	  m_filteredWS->mutableY(1).begin());
}

} // namespace Algorithm
} // namespace Mantid
