#ifndef SD_CrystalBase_h
#define SD_CrystalBase_h

#include "SD_Base.hh"
#include "ScinPhotonCollection.hh"
#include <numeric>

/*
 * Generic behavior for xtal detectors.
 */
template <typename Impl>
class SD_CrystalBase : public SD_Base<SD_CrystalBase<Impl>> {

  using BaseType = SD_Base<SD_CrystalBase<Impl>>;

public:
  SD_CrystalBase(const G4String& name, G4double length, G4double z_offset)
    : BaseType(name), SPCollection(100, length, z_offset)
  {
    // xtal branches and histograms
    b_depositedEnergy = CreateTree::Instance()->createBranch<float>(Impl::ID + "_depositedEnergy");
    b_depositedIonEnergy = CreateTree::Instance()->createBranch<float>(Impl::ID + "_depositedIonEnergy");
    b_ECAL_total = BaseType::createInt("total");
    b_ECAL_exit = BaseType::createInt("exit");
    h_phot_produce_lambda = BaseType::createHistogram("phot_produce_lambda", "Photon lambda production;[nm]", 1250, 0., 1250.);
    h_phot_produce_time = BaseType::createHistogram("phot_produce_time", "Photon time production;[ns]", 500, 0., 50.);
    h_phot_z_pos = BaseType::createHistogram("phot_z_pos", "Photon z production;[nm]", 500, 50., 300.);
    // h_phot_angle_azimuthal = BaseType::createHistogram("phot_angle_azimuthal", "Photon Azimuthal Angle;[rad]", 100, -M_PI, M_PI);
    // h_phot_angle_polar = BaseType::createHistogram("phot_angle_polar", "Photon Polar Angle;[rad]", 100, 0., M_PI);
    // h_phot_init_x = BaseType::createHistogram("phot_init_x", "Photon Init px", 100, -1.0, 1.0);
    // h_phot_init_y = BaseType::createHistogram("phot_init_y", "Photon Init py", 100, -1.0, 1.0);
    b_scin_bin_total = CreateTree::Instance()->createBranch<std::vector<int>>(Impl::ID + "_scin_bin_total");
    b_scin_bin_energy = CreateTree::Instance()->createBranch<std::vector<float>>(Impl::ID + "_scin_bin_energy");
  }

  void Initialize(G4HCofThisEvent*) override {
    *b_scin_bin_total = std::vector<int>(SPCollection.nBins(), 0);
    *b_scin_bin_energy = std::vector<float>(SPCollection.nBins(), 0.);
  }

  G4bool ProcessHits(G4Step* theStep, G4TouchableHistory* ) override {

    // Record energy deposition in the xtal
    const G4double energy = theStep->GetTotalEnergyDeposit();
    const G4double energyIon = energy - theStep->GetNonIonizingEnergyDeposit();

    *b_depositedEnergy += energy / GeV;
    *b_depositedIonEnergy += energyIon / GeV;

    handleOpticalPhoton(theStep,
      [this, theStep] (ProcessType process, float photWL, G4double gTime) {
        G4Track* track = theStep->GetTrack();

        // Only record production
        if (track->GetCurrentStepNumber() == 1) {
          const double sample = CLHEP::RandFlat::shoot(0.0, 1.0);
          const G4double zPos = (theStep->GetPreStepPoint()->GetPosition() + sample * theStep->GetDeltaPosition()).z();
          const G4double time = theStep->GetPreStepPoint()->GetGlobalTime() + sample * theStep->GetDeltaTime();

          b_ECAL_total[process]++;
          h_phot_produce_lambda[process].Fill(photWL);
          h_phot_produce_time[process].Fill(time);
          h_phot_z_pos[process].Fill(zPos);
          // h_phot_angle_azimuthal[process].Fill(track->GetMomentum().phi());
          // h_phot_angle_polar[process].Fill(track->GetMomentum().theta());

          // const auto p = track->GetMomentum();
          // h_phot_init_x[process].Fill(p.x() / p.mag());
          // h_phot_init_y[process].Fill(p.y() / p.mag());

          if (process == ProcessType::Scin) {
            SPCollection.recordCreation(track->GetTotalEnergy(), time, zPos);
          }
        }

        G4VPhysicalVolume *thePostPV = theStep->GetPostStepPoint()->GetPhysicalVolume();
        const G4String thePostPVName = thePostPV ? thePostPV->GetName() : "";

        // is photon leaving front face of xtal?
        if (thePostPVName.contains("matchBox") || thePostPVName.contains("baffle")){ 
          b_ECAL_exit[process]++;

          // kill track at baffle for now, no reflection
          if (thePostPVName.contains("baffle"))
            track->SetTrackStatus(fKillTrackAndSecondaries);
        }       
      });

    return true;
  }

  // G4bool ProcessHits_constStep(const G4Step*, G4TouchableHistory*) override {
  //   return false;
  // }

  void EndOfEvent(G4HCofThisEvent*) override {
    const auto& bins = SPCollection.GetBins();
    for (unsigned int i = 0; i < bins.size(); i++) {
      (*b_scin_bin_total)[i] = bins[i].size();
      for (const auto& hit : bins[i]) {
        (*b_scin_bin_energy)[i] += hit.energy / GeV;
      }
    }
  }

  void clear() override {}
  void DrawAll() override {}
  void PrintAll() override {}

  // ----- Naming functions

  static std::string HistogramName(const std::string& name, ProcessType type) {
    return "h_" + name + "_" + Impl::ID + "_" + processNameShort(type);
  }

  static std::string BranchName(const std::string& name, ProcessType type) {
    return Impl::ID + "_" + name + "_" + processNameInitial(type);
  }

private:
  float* b_depositedEnergy;
  float* b_depositedIonEnergy;
  PerProcess<int> b_ECAL_total;
  PerProcess<int> b_ECAL_exit;
  PerProcess<TH1F> h_phot_produce_lambda;
  PerProcess<TH1F> h_phot_produce_time;
  PerProcess<TH1F> h_phot_z_pos;
  PerProcess<TH1F> h_phot_angle_azimuthal;
  PerProcess<TH1F> h_phot_angle_polar;
  PerProcess<TH1F> h_phot_init_x;
  PerProcess<TH1F> h_phot_init_y;

  ScinPhotonCollection SPCollection;
  std::vector<int>* b_scin_bin_total;
  std::vector<float>* b_scin_bin_energy;
};

#endif
