////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////	CW_thres_debug.cxx
////	A23 diffuse, get number of events with CW contamination above certain threshold
////
////	Jorge Torres, Sept 30
////////////////////////////////////////////////////////////////////////////////

//C++
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include <sys/stat.h>

//ROOT Includes
#include "TTree.h"
#include "TFile.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"

//AraRoot includes
#include "RawAtriStationEvent.h"
#include "UsefulAtriStationEvent.h"
#include "Settings.h"
#include "Detector.h"
#include "Report.h"
#include "tools_PlottingFns.h"
#include "tools_RecoFns.h"
#include "tools_inputParameters.h"
#include "tools_outputObjects.h"
#include "tools_Cuts.h"
#include "tools_CommandLine.h"

using namespace std;

int main(int argc, char **argv)
{
	gStyle->SetOptStat(0);
	time_t time_now = time(0); //get the time now
	tm *time = localtime(&time_now);
	int year_now = time -> tm_year + 1900;
	int month_now = time -> tm_mon + 1;
	int day_now = time -> tm_mday;

	char *AuxDirPath(getenv("AUX_DIR"));
	if (AuxDirPath == NULL){
		std::cout << "Warning! $AUX_DIR is not set! You need this for CWID and RunSummary files" << endl;
		return -1;
	}
	char *SimDirPath(getenv("SIM_DIR"));
	if (SimDirPath == NULL){
		std::cout << "Warning! $SIM_DIR is not set!" << endl;
		return -1;
	}
	char *PedDirPath(getenv("PED_DIR"));
	if (PedDirPath == NULL){
		std::cout << "Warning! $DATA_DIR is not set!" << endl;
		return -1;
	}
	char *plotPath(getenv("PLOT_PATH"));
	if (plotPath == NULL){
		std::cout << "Warning! $PLOT_PATH is not set!" << endl;
		return -1;
	}

	stringstream ss;
	AraEventCalibrator *calibrator = AraEventCalibrator::Instance();

	if(argc<8){
		cout<< "Usage\n" << argv[0] << " <isSim?> <station> <config> <year_or_energy (as float, eg 17.0 or 18.5)> <drop_bad_chan> <output_location> <joined filename 1> <joined filename 2 > ... <joined filename x>"<<endl;
		return 0;
	}
	int isSimulation = atoi(argv[1]);
	int station = atoi(argv[2]);
	int config = atoi(argv[3]);
	int year_or_energy = atoi(argv[4]);
	int dropBadChans = atoi(argv[5]);
	string output_location = argv[6];
	char histoName [100];
	char histoName2 [100];
	sprintf(histoName, "Tagged freqs per event, A%i, c%i, vpol", station, config);
	sprintf(histoName2, "Tagged freqs per event, A%i, c%i, hpol", station, config);

	TH2F * hist_badfreq[2];
	if(config==1){
		hist_badfreq[0] = new TH2F(histoName, histoName, 500,1350,2000,1024,0,1000);
		hist_badfreq[1] = new TH2F(histoName2, histoName2, 500,1350,2000,1024,0,1000);
	}
	else if(config==2){
		hist_badfreq[0] = new TH2F(histoName, histoName, 500,400,1500,1024,0,1000);
		hist_badfreq[1] = new TH2F(histoName2, histoName2, 500,400,1500,1024,0,1000);
	}
	else if(config==3){
		hist_badfreq[0] = new TH2F(histoName, histoName, 500,3000,8000,1024,0,1000);
		hist_badfreq[1] = new TH2F(histoName2, histoName2, 500,3000,8000,1024,0,1000);
	}
	else if(config==4){
		hist_badfreq[0] = new TH2F(histoName, histoName, 500,6000,8000,1024,0,1000);
		hist_badfreq[1] = new TH2F(histoName2, histoName2, 500,6000,8000,1024,0,1000);
	}
	else if(config==5){
		hist_badfreq[0] = new TH2F(histoName, histoName, 500,1500,3500,1024,0,1000);
		hist_badfreq[1] = new TH2F(histoName2, histoName2, 500,1500,3500,1024,0,1000);
	}
	char filename[100];
  sprintf(filename, "CWID_A%i_c%i_th1.5.csv", station, config);//,angle[i]);
  // else sprintf(filename, "wfront_RMS_cut_A%i_c%i_sim.csv", station,config);
  FILE *fout_RMS = fopen(filename, "w+");//open file
	int runNum=0;
	// TH1F *hist_freq = new TH1F("","",300,0,1000);
	vector<int> BadRunList = BuildBadRunList(3);
	for(int file_num=7; file_num<argc; file_num++){//7
		int num_tot_events=0;
	  int num_total_filtered_back=0;
	  int num_total_filtered_fwd=0;
		int baseline_count = 0;

		string file = string(argv[file_num]);
		string wordRun = "run_";
		size_t foundRun = file.find(wordRun);
		string wordFilter = "_joined";
		size_t foundFilter = file.find(wordFilter);
		size_t diff=(foundFilter-wordRun.length())-foundRun;
		string strRunNum = file.substr(foundRun+4,diff);
		runNum = atoi(strRunNum.c_str());
		if(isBadRun(3, runNum, BadRunList)) continue;
		if(isSoftwareDominatedRun("/users/PCON0003/cond0068/ARA/AraRoot/analysis/a23_analysis_tools", station, runNum)) continue;
		// if(config==3 && (runNum>3500)) continue;
		// if(config==4 && (/*runNum<6000 || */runNum<6500)) continue;
		// if(config==5 && runNum>2200) continue;

		cout << runNum << endl;
		double freqTest;

		if(!isSimulation){
			//we're almost certainly going to need the calibrator, so let's just load it now
			char ped_file_name[400];
			sprintf(ped_file_name,"%s/run_specific_peds/A%d/all_peds/event%d_specificPeds.dat",PedDirPath,station,runNum);
			calibrator->setAtriPedFile(ped_file_name,station); //because someone had a brain (!!), this will error handle itself if the pedestal doesn't exist
		}
    // char histName[50];
    // sprintf(histName,"400 MHz phase variance factor, Run %d, pol=0", runNum);
    // TH1F * hist = new TH1F("hist",histName,100,0,5);
		// cout << "Run " << file_num << " :: " << argv[file_num] << endl;

		TFile *inputFile = TFile::Open(argv[file_num]);
		if(!inputFile){
			cout<<"Can't open joined file!"<<endl;
			return -1;
		}

		//next, we need to load the filter tree
		ss.str("");
		ss << "OutputTree_filter";
		TTree *inputTree_filter = (TTree*) inputFile->Get(ss.str().c_str());
		if(!inputTree_filter){
			cout<<"Can't open filter tree"<<endl;
			return -1;
		}
		bool isCalPulser_in;
		bool isSoftTrigger_in;
		int waveformLength_in[16];
		bool hasDigitizerError_in;
		double inweight;
		int Trig_Pass_in[16];
		int unixTime_in;
		int eventNumber_in;
    inputTree_filter->SetBranchAddress("isSoftTrigger",&isSoftTrigger_in);


		inputTree_filter->SetBranchAddress("VPeakOverRMS",&VPeakOverRMS); //get SNR's of all channels
		inputTree_filter->SetBranchAddress("thirdVPeakOverRMS", &thirdVPeakOverRMS);
		inputTree_filter->SetBranchAddress("rms_pol_thresh_face_V", &rms_pol_thresh_face_V);
		inputTree_filter->SetBranchAddress("rms_pol_thresh_face_H", &rms_pol_thresh_face_H);
		inputTree_filter->SetBranchAddress("weight", &inweight);
		inputTree_filter->SetBranchAddress("unixTime",&unixTime_in);
		inputTree_filter->SetBranchAddress("eventNumber",&eventNumber_in);
		if(isSimulation)
			inputTree_filter->SetBranchAddress("Trig_Pass", &Trig_Pass_in);

		int numFaces_new_V;
		int numFaces_new_H;
		if(station==2){
			numFaces_new_V = numFaces;
			numFaces_new_H = numFaces_A2_drop;
		}
		else if(station==3){
			numFaces_new_V = numFaces_A3_drop;
			numFaces_new_H = numFaces_A3_drop;
		}
		double rms_pol_thresh_face_alternate_V[thresholdSteps][numFaces_new_V];
		double rms_pol_thresh_face_alternate_H[thresholdSteps][numFaces_new_H];
		inputTree_filter->SetBranchAddress("rms_pol_thresh_face_alternate_V", &rms_pol_thresh_face_alternate_V);
		inputTree_filter->SetBranchAddress("rms_pol_thresh_face_alternate_H", &rms_pol_thresh_face_alternate_H);

		inputTree_filter->SetBranchAddress("isCalpulser",&isCalPulser_in);
		inputTree_filter->SetBranchAddress("isSoftTrigger",&isSoftTrigger_in);
		inputTree_filter->SetBranchAddress("waveformLength",&waveformLength_in);
		inputTree_filter->SetBranchAddress("hasDigitizerError",&hasDigitizerError_in);

		// copy over the whole filter tree
		//fpOut->cd();
		char summary_file_name[400];
		if(isSimulation){
			if(year_or_energy<25)
				sprintf(summary_file_name,"%s/CWID/A%d/c%d/E%2.2f/CWID_station_%d_run_%d.root",SimDirPath,station,config,year_or_energy,station,runNum);
			else
				sprintf(summary_file_name,"%s/CWID/A%d/c%d/E%d/CWID_station_%d_run_%d.root",SimDirPath,station,config,int(year_or_energy),station,runNum);
		}
		else{
			sprintf(summary_file_name,"%s/CWID/A%d/by_config/c%d/CWID_station_%d_run_%d.root",AuxDirPath,station,config,station,runNum);
		}
		TFile *NewCWFile = TFile::Open(summary_file_name);
		if(!NewCWFile) {
			std::cerr << "Can't open new CW file\n";
			return -1;
		}
		TTree* NewCWTree = (TTree*) NewCWFile->Get("NewCWTree");
		if(!NewCWTree) {
			std::cerr << "Can't find NewCWTree\n";
			return -1;
		}
		vector<vector<double> > *badFreqs_fwd =0;
		vector<vector<double> > *badFreqs_back=0;
		vector<vector<double> > *badSigmas_fwd=0;
		vector<vector<double> > *badSigmas_back=0;
		vector<vector<double> > *badFreqs_baseline=0;

		NewCWTree->SetBranchAddress("badFreqs_fwd",&badFreqs_fwd);
		NewCWTree->SetBranchAddress("badSigmas_fwd",&badSigmas_fwd);
		NewCWTree->SetBranchAddress("badFreqs_back",&badFreqs_back);
		NewCWTree->SetBranchAddress("badSigmas_back",&badSigmas_back);
		NewCWTree->SetBranchAddress("badFreqs_baseline",&badFreqs_baseline);

		double threshCW=10;
		if(station==2){
			threshCW = 1.5;
		}
		else if(station==3){
			// cout << hasUntaggedCalpul("./a23_analysis_tools",station,config, runNum) << endl;
			if(hasUntaggedCalpul("./a23_analysis_tools",station,config, runNum)) threshCW = 1.5;
			else threshCW = 1.5;
		}
		// cout << threshCW << endl;
		int numEntries = inputTree_filter->GetEntries();
    num_tot_events+=numEntries;
		Long64_t starEvery=numEntries/200;
		if(starEvery==0) starEvery++;
		// cout<<"Num entries is "<<numEntries<<endl;
		// cout<<"Star every is "<<starEvery<<endl;
		// // numEntries=2000;

		int start=0;
		//now to loop over events
		for(int event=start; event<numEntries; event++){
			if(event%starEvery==0) {
				// std::cout<<"*";
			}

			// reset the variables that we are passing *out*

			// things that don't require loops
			int isCal_out=0;
			int isSoft_out=0;
			int isShortWave_out=0;
			int isCW_out=0;
			int isNewBox_out=0;
			bool isFirstFiveEvent_out=false;
			bool hasBadSpareChanIssue_out=false;
			bool hasBadSpareChanIssue2_out=false;

			inputTree_filter->GetEvent(event);

			if(isBadLivetime(station, unixTime_in)) continue;


			bool isShort=false;
			bool isSurf[2]={false};
			bool isCP5=false;
			bool isCP6=false;
			bool failWavefrontRMS[2];
			failWavefrontRMS[0]=false;
			failWavefrontRMS[1]=false;

			//deal w/ CW cut
			//inputTree_CW->GetEntry(event);
			NewCWTree->GetEntry(event);

			bool isCutonCW_fwd[2]; isCutonCW_fwd[0]=false; isCutonCW_fwd[1]=false;
			bool isCutonCW_back[2]; isCutonCW_back[0]=false; isCutonCW_back[1]=false;
			bool isCutonCW_baseline[2]; isCutonCW_baseline[0]=false; isCutonCW_baseline[1]=false;

			for(int pol=0; pol<badFreqs_baseline->size(); pol++){
				vector<double> badFreqListLocal_baseline = badFreqs_baseline->at(pol);
				if(badFreqListLocal_baseline.size()>0) isCutonCW_baseline[pol]=true;
			}
			if(isCutonCW_baseline[0]==true || isCutonCW_baseline[1]==true) baseline_count++;

			double count_tagged_freq_h=0;
			double count_tagged_freq_v=0;
      bool isFiltered_fwd=false;
			vector<double> badFreqList_fwd;
			vector<double> badSigmaList_fwd;
			for(int pol=0; pol<badFreqs_fwd->size(); pol++){
				badFreqList_fwd=badFreqs_fwd->at(pol);
				badSigmaList_fwd=badSigmas_fwd->at(pol);
				for(int iCW=0; iCW<badFreqList_fwd.size(); iCW++){
					if ((int) badSigmaList_fwd[iCW] % 25 >22 || (int) badSigmaList_fwd[iCW] % 25 <3) continue;
					else if(
						badSigmaList_fwd[iCW] > threshCW
						&&
						abs(300. - badFreqList_fwd[iCW]) > 2.
						// abs(403.3 - badFreqList_fwd[iCW]) > 4.
						&&
						abs(500. - badFreqList_fwd[iCW]) > 2.
						&&
						abs(125. - badFreqList_fwd[iCW]) > 2.
						// &&
						// ((int) badSigmaList_fwd[iCW] % 25 <22 && (int) badSigmaList_fwd[iCW] % 25 >3)
						&&
						badFreqList_fwd[iCW] < 850.
					){
						// hist_freq->Fill(badFreqList_fwd[iCW]);
						// cout << badFreqList_fwd[iCW] << endl;
						isCutonCW_fwd[pol] = true;

						// if(pol==0) count_tagged_freq_v++;
						// if(pol==1) count_tagged_freq_h++;

						if(pol==0) hist_badfreq[0]->Fill(runNum,badFreqList_fwd[iCW]);
						if(pol==1) hist_badfreq[1]->Fill(runNum,badFreqList_fwd[iCW]);

            if(!isFiltered_fwd) num_total_filtered_fwd++;
            isFiltered_fwd=true;
					}
				}
			}
			// hist_badfreq[0]->Fill(runNum,count_tagged_freq_v);
			// hist_badfreq[1]->Fill(runNum,count_tagged_freq_h);

      // cout << unixTime_in << endl;
      bool isFiltered_back=false;
			vector<double> badFreqList_back;
			vector<double> badSigmaList_back;
      vector<double> peak_phase;
			for(int pol=0; pol<badFreqs_back->size(); pol++){
        // cout << badFreqs_back->size() << endl;
				badFreqList_back=badFreqs_back->at(pol);
				badSigmaList_back=badSigmas_back->at(pol);
				for(int iCW=0; iCW<badFreqList_back.size(); iCW++){
          if(abs(403.3 - badFreqList_back[iCW]) < 4 && (pol==0)){
            // cout << event << endl;
            // cout << badFreqList_back[iCW] << endl;
            // // cout << badFreqList_back[iCW] << endl;
            peak_phase.push_back(badSigmaList_back[iCW]);
          }
					if(
						badSigmaList_back[iCW] > threshCW
						&&
						abs(300. - badFreqList_back[iCW]) > 2.
						&&
						abs(500. - badFreqList_back[iCW]) > 2.
						&&
						abs(125. - badFreqList_back[iCW]) > 2.
						&&
						badFreqList_back[iCW] < 850.
					){
						isCutonCW_back[pol] = true;
						// cout<<"The bad frequency mode is "<<badFreqList_back[iCW]<<endl;
            if(!isFiltered_back){
              num_total_filtered_back++;
            }
            isFiltered_back=true;
            // if(!isSoftTrigger_in) hist->Fill(badFreqList_back[iCW]);
					}
				}
			}
      sort(peak_phase.begin(), peak_phase.end(), greater<double>());
      if(peak_phase.size()!=0){
        // hist->Fill(peak_phase[0]);
      }
		}//loop over events
		inputFile->Close();
		NewCWFile->Close();
		delete inputFile;

    // char outfile_name[300];
		// sprintf(outfile_name,"%s/CW_contaminated_events_run_%d.root",output_location.c_str(),runNum);
    //
		// // TFile *fpOut = new TFile(outfile_name,"recreate");


    // hist->Write();
		// fpOut->Write();
		// fpOut->Close();
		// delete fpOut;
		fprintf(fout_RMS,"%i,%0.2f,%0.2f\n",runNum, (float) 100*num_total_filtered_back/num_tot_events, (float) 100*baseline_count/num_tot_events);
		// cout <<runNum<<endl;
		printf("Done! Run Number %d \n", runNum);

	} //end loop over input files

	fclose(fout_RMS);
	// TFile *outputFile;

  // outputFile = new TFile("./bad_freqs_histograms.root","RECREATE");
  // //hPedEvent->Write(); //***** I tried this one but error
  // for(int iBin=0; iBin<2; iBin++){
  //   hist_badfreq[iBin]->Write(); //***** I tried this one, the software can run, but I cannot call the histogram when opening root file
  //   std::cout << "stored hPedEvent[" << iBin << "] in " << "./bad_freqs_histograms.root" << std::endl;
  // }
  // outputFile->Close();
  // printf("Num events = %i, num filtered events backward = %i, ratio = %f \n", num_tot_events, num_total_filtered_back, (float) num_total_filtered_back/num_tot_events);
  // printf("Num events = %i, num filtered events fwd = %i, ratio = %f \n", num_tot_events, num_total_filtered_fwd, (float) num_total_filtered_fwd/num_tot_events);
	TCanvas *cc = new TCanvas("","",2500,1000*2);
	cc->Divide(1,2);
	cc->cd(1);
	hist_badfreq[0]->GetXaxis()->SetTitle("Run Number");
	hist_badfreq[0]->GetYaxis()->SetTitle("Number of tagged frequencies");
	hist_badfreq[0]->GetZaxis()->SetTitle("Counts");
	hist_badfreq[0]->Draw("colz");
	gStyle->SetOptStat(1111);
	gPad->SetLogz();
	gStyle->SetOptStat(1111);
	cc->SetRightMargin(0.1);

	cc->cd(2);
	hist_badfreq[1]->GetXaxis()->SetTitle("Run Number");
	hist_badfreq[1]->GetYaxis()->SetTitle("Tagged frequencies");
	hist_badfreq[1]->GetZaxis()->SetTitle("Counts");
	hist_badfreq[1]->Draw("colz");
	gStyle->SetOptStat(1111);
	gPad->SetLogz();
	gStyle->SetOptStat(1111);
	cc->SetRightMargin(0.1);
	char h5name[60];
	sprintf(h5name,"/users/PCON0003/cond0068/ARA/AraRoot/analysis/plots/badFreq_hist/badFreq_hist_perRun_c%d_A%d_th1.5.png",config, station);
	cc->SaveAs(h5name);
	// delete hist_badfreq[0];
	// delete hist_badfreq[1];
	delete cc;

	TH1D * projh2X[2];
	TH1D * projh2Y[2];
	TCanvas *cc_proj = new TCanvas("","",2000*2,1000*2);
	cc_proj->Divide(2,2);
	cc_proj->cd(1);
	projh2X[0] = hist_badfreq[0]->ProjectionX();
	projh2X[0]->GetXaxis()->SetTitle("Run Number");
	projh2X[0]->GetYaxis()->SetTitle("Count");
	gPad->SetLogy();
	projh2X[0]->SetFillColor(kBlue+1);
	projh2X[0]->Draw("bar");
	gStyle->SetOptStat(1111);

	cc_proj->cd(2);
	projh2Y[0] = hist_badfreq[0]->ProjectionY();
	projh2Y[0]->GetXaxis()->SetTitle("Tagged freqs per event");
	projh2Y[0]->GetYaxis()->SetTitle("Count");
	projh2Y[0]->SetFillColor(kBlue-2);
	gPad->SetLogy();
	projh2Y[0]->Draw("bar");
	gStyle->SetOptStat(1111);

	cc_proj->cd(3);
	projh2X[1] = hist_badfreq[1]->ProjectionX();
	projh2X[1]->GetXaxis()->SetTitle("Run Number");
	projh2X[1]->GetYaxis()->SetTitle("Count");
	projh2X[1]->SetFillColor(kBlue+1);
	gPad->SetLogy();
	projh2X[1]->Draw("bar");
	gStyle->SetOptStat(1111);

	cc_proj->cd(4);
	projh2Y[1] = hist_badfreq[1]->ProjectionY();
	projh2Y[1]->GetXaxis()->SetTitle("Tagged freqs per event");
	projh2Y[1]->GetYaxis()->SetTitle("Count");
	projh2Y[1]->SetFillColor(kBlue-2);
	gPad->SetLogy();
	projh2Y[1]->Draw("bar");
	gStyle->SetOptStat(1111);

	char h6name[60];
	sprintf(h6name,"/users/PCON0003/cond0068/ARA/AraRoot/analysis/plots/badFreq_hist/badFreq_proj_perRun_c%d_A%d_th1.5.png",config, station);
	cc_proj->SaveAs(h6name);
	// delete hist_badfreq[0];
	// delete hist_badfreq[1];
	delete cc_proj;
}
