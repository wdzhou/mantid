#pylint: disable=no-init
"""
    Extract or compute the Q values from reduced QENS data
"""
from stresstesting import MantidStressTest
import mantid
import mantid.simpleapi as sm
import re


class GlobalFitTest(MantidStressTest):
    """Global fit of QENS data to the jump-diffusion model by Teixeira
       Fitting model. In this case:
        Convolution( A*Resolution, x*Delta + (1-x)*TeixeiraWaterSQE ) + LinearBackground
        with 0<x<1 is the fraction of the elastic intensity
       Parameters DiffCoeff and Tau (residence time) of fit function TeixeiraWaterSQE are
       the same for all spectra. All other fitting  parameters are different for each spectrum
    """

    #def requiredFiles(self):
    #    return ["irs26173_graphite002_res.nxs", "irs26176_graphite002_red.nxs"]

    def runTest(self):

        data = sm.Load("irs26176_graphite002_red.nxs")
        sm.Load("irs26173_graphite002_res.nxs", OutputWorkspace="resolution")

        single_model = """(composite=Convolution,NumDeriv=true;
        name=TabulatedFunction,Workspace=resolution,WorkspaceIndex=0,
             Scaling=1,Shift=0,XScaling=1;
        (name=DeltaFunction,Height=1.5,Centre=0;
         name=TeixeiraWaterSQE,Height=1.0,Tau=1.0,DiffCoeff=1.0,Centre=0;
         ties=(f1.Centre=f0.Centre)));
        name=LinearBackground,A0=0,A1=0"""
        single_model = re.sub('[\s+]', '', single_model)

        # Include all spectra for the fit
        selected_wi = range(data.getNumberHistograms())
        nWi = len(selected_wi)  # number of selected spectra for fitting

        # Energy range over which we do the fitting.
        minE = -0.4  # Units are in meV
        maxE = 0.4

        # Create the string representation of the global model (for selected spectra):
        global_model="composite=MultiDomainFunction,NumDeriv=true;"
        for _ in selected_wi:
            global_model += "(composite=CompositeFunction,NumDeriv=true,$domains=i;{0});\n".format(single_model)

        # Tie DiffCoeff and Tau since they are global parameters
        ties = ['='.join(["f{0}.f0.f1.f1.DiffCoeff".format(di) for di in reversed(range(nWi))]),
                '='.join(["f{0}.f0.f1.f1.Tau".format(wi) for wi in reversed(range(nWi))])]
        global_model += "ties=(" + ','.join(ties) + ')'  # tie Radius

        # Now relate each domain(i.e. spectrum) to each single-spectrum model
        domain_model = dict()
        domain_index = 0
        for wi in selected_wi:
            if domain_index == 0:
                domain_model.update({"InputWorkspace": data.name(), "WorkspaceIndex": str(wi),
                                     "StartX": str(minE), "EndX": str(maxE)})
            else:
                di = str(domain_index)
                domain_model.update({"InputWorkspace_" + di: data.name(), "WorkspaceIndex_" + di: str(wi),
                                     "StartX_" + di: str(minE), "EndX_" + di: str(maxE)})
            domain_index += 1

        # Invoke the Fit algorithm using global_model and domain_model:
        output_workspace = "glofit_" + data.name()
        status,chi2,covar,params,curves = sm.Fit(Function=global_model, Output=output_workspace,
                                                 CreateOutput=True, MaxIterations=500,
                                                 **domain_model)

        # Validate
        self._success = True
        try:
            self.assertTrue(chi2 < 1)
            self.assertTrue(abs(params.row(6)["Value"]-1.58)/1.58 < 0.1) # optimal DiffCoeff
            self.assertTrue(abs(params.row(7)["Value"] - 1.16) / 1.16 < 0.1)  # optimal Tau
        except:
            self._success = False

    def cleanup(self):
        for workspace in ["data", "resolution", "glofit_data_Parameters",
                          "glofit_data_Workspaces", "glofit_data_NormalisedCovarianceMatrix"]:
            if mantid.AnalysisDataService.doesExist(workspace):
                sm.DeleteWorkspace(workspace)

    def validate(self):
        return self._success
