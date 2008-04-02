#ifndef UNITTEST_H_
#define UNITTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidKernel/Unit.h"

using namespace Mantid::Kernel;

class UnitTest : public CxxTest::TestSuite
{
  class UnitTester : public Unit
  {
  public:
    UnitTester() : Unit() {}
    virtual ~UnitTester() {}
    
    // Empty overrides of virtual methods
    const int unitCode() const {return 0;}
    const std::string caption() const {return "";}
    const std::string label() const {return "";}
    void toTOF(std::vector<double>&,std::vector<double>&,const double&,const double&,const double&,const int&,const double&,const double&) const {}
    void fromTOF(std::vector<double>&,std::vector<double>&,const double&,const double&,const double&,const int&,const double&,const double&) const {}
  };
  
public:
  
  //----------------------------------------------------------------------
  // Base Unit class tests
  //----------------------------------------------------------------------
  
	void testUnit_GetSetDescription()
	{
	  UnitTester t;
    TS_ASSERT_EQUALS( t.description(), "" )
    t.setDescription("testing");
    TS_ASSERT_EQUALS( t.description(), "testing" )
	}

  //----------------------------------------------------------------------
  // TOF tests
  //----------------------------------------------------------------------

	void testTOF_unitCode()
	{
		TS_ASSERT_EQUALS( tof.unitCode(), 1 )
	}

	void testTOF_caption()
	{
		TS_ASSERT_EQUALS( tof.caption(), "Time-of-flight" )
	}

	void testTOF_label()
	{
	  TS_ASSERT_EQUALS( tof.label(), "microsecond" )
	}
	
	void testTOF_cast()
	{
	  TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(tof) )
	}

	void testTOF_toTOF()
	{
	  std::vector<double> x(20,9.9), y(20,8.8);
	  std::vector<double> xx = x;
    std::vector<double> yy = y;
	  TS_ASSERT_THROWS_NOTHING( tof.toTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
	  // Check vectors are unchanged
	  TS_ASSERT( xx == x )
	  TS_ASSERT( yy == y )
	}

	void testTOF_fromTOF()
	{
    std::vector<double> x(20,9.9), y(20,8.8);
    std::vector<double> xx = x;
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( tof.fromTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    // Check vectors are unchanged
    TS_ASSERT( xx == x )
    TS_ASSERT( yy == y )
	}

  //----------------------------------------------------------------------
  // Wavelength tests
  //----------------------------------------------------------------------

	void testWavelength_unitCode()
	{
    TS_ASSERT_EQUALS( lambda.unitCode(), 2 )
	}

	void testWavelength_caption()
	{
    TS_ASSERT_EQUALS( lambda.caption(), "Wavelength" )
	}

	void testWavelength_label()
	{
    TS_ASSERT_EQUALS( lambda.label(), "Angstrom" )
	}

	void testWavelength_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(lambda) )
  }

	void testWavelength_toTOF()
	{
    std::vector<double> x(1,1.5), y(1,1.5);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( lambda.toTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 758.3352, 0.0001 )
    TS_ASSERT( yy == y )
	}

	void testWavelength_fromTOF()
	{
    std::vector<double> x(1,1000.5), y(1,1.5);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( lambda.fromTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 1.979006, 0.000001 )    
    TS_ASSERT( yy == y )
	}

	//----------------------------------------------------------------------
  // Energy tests
  //----------------------------------------------------------------------

  void testEnergy_unitCode()
  {
    TS_ASSERT_EQUALS( energy.unitCode(), 3 )
  }

  void testEnergy_caption()
  {
    TS_ASSERT_EQUALS( energy.caption(), "Energy" )
  }

  void testEnergy_label()
  {
    TS_ASSERT_EQUALS( energy.label(), "MeV" )
  }

  void testEnergy_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(energy) )
  }

  void testEnergy_toTOF()
  {
    std::vector<double> x(1,4.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( energy.toTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 2286.271, 0.001 )
    TS_ASSERT( yy == y )
  }

  void testEnergy_fromTOF()
  {
    std::vector<double> x(1,4.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( energy.fromTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 1306759.0, 1.0 )
    TS_ASSERT( yy == y )
  }

  //----------------------------------------------------------------------
  // d-Spacing tests
  //----------------------------------------------------------------------

  void testdSpacing_unitCode()
  {
    TS_ASSERT_EQUALS( d.unitCode(), 4 )
  }

  void testdSpacing_caption()
  {
    TS_ASSERT_EQUALS( d.caption(), "d-Spacing" )
  }

  void testdSpacing_label()
  {
    TS_ASSERT_EQUALS( d.label(), "Angstrom" )
  }

  void testdSpacing_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(d) )
  }

  void testdSpacing_toTOF()
  {
    std::vector<double> x(1,1.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( d.toTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 484.7537, 0.0001 )
    TS_ASSERT( yy == y )
  }

  void testdSpacing_fromTOF()
  {
    std::vector<double> x(1,1001.1), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( d.fromTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 2.065172, 0.000001 )
    TS_ASSERT( yy == y )
  }

  //----------------------------------------------------------------------
  // Momentum Transfer tests
  //----------------------------------------------------------------------

  void testQTransfer_unitCode()
  {
    TS_ASSERT_EQUALS( q.unitCode(), 5 )
  }

  void testQTransfer_caption()
  {
    TS_ASSERT_EQUALS( q.caption(), "q" )
  }

  void testQTransfer_label()
  {
    TS_ASSERT_EQUALS( q.label(), "1/Angstrom" )
  }

  void testQTransfer_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(q) )
  }

  void testQTransfer_toTOF()
  {
    std::vector<double> x(1,1.1), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( q.toTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 2768.9067, 0.0001 )
    TS_ASSERT( yy == y )
  }

  void testQTransfer_fromTOF()
  {
    std::vector<double> x(1,1.1), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( q.fromTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 2768.9067, 0.0001 )
    TS_ASSERT( yy == y )
  }

  //----------------------------------------------------------------------
  // Momentum Squared tests
  //----------------------------------------------------------------------

  void testQ2_unitCode()
  {
    TS_ASSERT_EQUALS( q2.unitCode(), 6 )
  }

  void testQ2_caption()
  {
    TS_ASSERT_EQUALS( q2.caption(), "Q2" )
  }

  void testQ2_label()
  {
    TS_ASSERT_EQUALS( q2.label(), "Angstrom^-2" )
  }

  void testQ2_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(q2) )
  }

  void testQ2_toTOF()
  {
    std::vector<double> x(1,4.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( q2.toTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 1522.899, 0.001 )
    TS_ASSERT( yy == y )
  }

  void testQ2_fromTOF()
  {
    std::vector<double> x(1,200.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( q2.fromTOF(x,y,1.0,1.0,1.0,1,1.0,1.0) )
    TS_ASSERT_DELTA( x[0], 231.9220, 0.0001 )
    TS_ASSERT( yy == y )
  }

  //----------------------------------------------------------------------
  // Energy transfer tests
  //----------------------------------------------------------------------

  void testDeltaE_unitCode()
  {
    TS_ASSERT_EQUALS( dE.unitCode(), 7 )
  }

  void testDeltaE_caption()
  {
    TS_ASSERT_EQUALS( dE.caption(), "Energy transfer" )
  }

  void testDeltaE_label()
  {
    TS_ASSERT_EQUALS( dE.label(), "meV" )
  }

  void testDeltaE_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(dE) )
  }

  void testDeltaE_toTOF()
  {
    std::vector<double> x(1,1.1), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( dE.toTOF(x,y,1.5,2.5,0.0,1,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], 5071.066, 0.001 )
    TS_ASSERT( yy == y )

    x[0] = 1.1;
    TS_ASSERT_THROWS_NOTHING( dE.toTOF(x,y,1.5,2.5,0.0,2,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], 4376.406, 0.001 )
    TS_ASSERT( yy == y )

    // emode = 0
    TS_ASSERT_THROWS( dE.toTOF(x,y,1.5,2.5,0.0,0,4.0,0.0), std::invalid_argument )
  }

  void testDeltaE_fromTOF()
  {
    std::vector<double> x(1,2001.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( dE.fromTOF(x,y,1.5,2.5,0.0,1,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], -394.5692, 0.0001 )
    TS_ASSERT( yy == y )

    x[0] = 3001.0;
    TS_ASSERT_THROWS_NOTHING( dE.fromTOF(x,y,1.5,2.5,0.0,2,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], 569.8397, 0.0001 )
    TS_ASSERT( yy == y )

    // emode = 0
    TS_ASSERT_THROWS( dE.fromTOF(x,y,1.5,2.5,0.0,0,4.0,0.0), std::invalid_argument )
}

  //----------------------------------------------------------------------
  // Energy transfer tests
  //----------------------------------------------------------------------

  void testDeltaEk_unitCode()
  {
    TS_ASSERT_EQUALS( dEk.unitCode(), 8 )
  }

  void testDeltaEk_caption()
  {
    TS_ASSERT_EQUALS( dEk.caption(), "Energy transfer" )
  }

  void testDeltaEk_label()
  {
    TS_ASSERT_EQUALS( dEk.label(), "1/cm" )
  }

  void testDeltaEk_cast()
  {
    TS_ASSERT_THROWS_NOTHING( dynamic_cast<Unit&>(dEk) )
  }

  void testDeltaEk_toTOF()
  {
    std::vector<double> x(1,1.1), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( dEk.toTOF(x,y,1.5,2.5,0.0,1,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], 11246.74, 0.01 )
    TS_ASSERT( yy == y )

    x[0] = 1.1;
    TS_ASSERT_THROWS_NOTHING( dEk.toTOF(x,y,1.5,2.5,0.0,2,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], 7170.555, 0.001 )
    TS_ASSERT( yy == y )

    // emode = 0
    TS_ASSERT_THROWS( dEk.toTOF(x,y,1.5,2.5,0.0,0,4.0,0.0), std::invalid_argument )
  }

  void testDeltaEk_fromTOF()
  {
    std::vector<double> x(1,2001.0), y(1,1.0);
    std::vector<double> yy = y;
    TS_ASSERT_THROWS_NOTHING( dEk.fromTOF(x,y,1.5,2.5,0.0,1,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], -3182.416, 0.001 )
    TS_ASSERT( yy == y )

    x[0] = 3001.0;
    TS_ASSERT_THROWS_NOTHING( dEk.fromTOF(x,y,1.5,2.5,0.0,2,4.0,0.0) )
    TS_ASSERT_DELTA( x[0], 4596.068, 0.001 )
    TS_ASSERT( yy == y )

    // emode = 0
    TS_ASSERT_THROWS( dEk.fromTOF(x,y,1.5,2.5,0.0,0,4.0,0.0), std::invalid_argument )
  }

private:
  Units::TOF tof;
  Units::Wavelength lambda;
  Units::Energy energy;
  Units::dSpacing d;
	Units::MomentumTransfer q;
	Units::QSquared q2;
	Units::DeltaE dE;
	Units::DeltaE_inWavenumber dEk;
};

#endif /*UNITTEST_H_*/
