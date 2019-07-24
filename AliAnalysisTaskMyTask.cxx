/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

/* AliAnaysisTaskMyTask
 *
 * empty task which can serve as a starting point for building an analysis
 * as an example, one histogram is filled
 */

#include "TSystem.h"
#include "TFile.h"
#include "TChain.h"
#include "TH1F.h"
#include "TH3.h"
#include "TTree.h"
#include "TNtuple.h"
#include "TList.h"

#include "AliESDEvent.h"
#include "AliESDtrack.h"
#include "AliESDTrdTrack.h"
#include "AliESDTrdTracklet.h"
#include "AliESDInputHandler.h"

#include "AliTRDdigitsManager.h"
#include "AliTRDgeometry.h"
#include "AliTRDarrayADC.h"

#include "AliAnalysisTask.h"
#include "AliAnalysisManager.h"
#include "AliAnalysisTaskSE.h"
#include "AliAnalysisTaskMyTask.h"

#include <math.h>
#include <iostream>
#include <fstream>
using namespace std; // std namespace: so you can do things like 'cout'

class AliAnalysisTaskMyTask; // your analysis class

ClassImp(AliAnalysisTaskMyTask) // classimp: necessary for root

AliAnalysisTaskMyTask::AliAnalysisTaskMyTask() : AliAnalysisTaskSE(),
    fESD(0), fOutputList(0), fHistPt(0), 
    summary(0), eventCount(0), minY(0), maxY(0),
    fTracklet(0), mp(0), fDigMan(0), fGeo(0),
    fDigitsInputFileName("TRD.FltDigits.root"), 
    fDigitsInputFile(0), fEventNoInFile(0),
    fOutputPath("/mnt/jsroot/data.min.js")
{
    // default constructor, don't allocate memory here!
    // this is used by root for IO purposes, it needs to remain empty
}
//_____________________________________________________________________________
AliAnalysisTaskMyTask::AliAnalysisTaskMyTask(const char *name) : AliAnalysisTaskSE(name),
    fESD(0), fOutputList(0), fHistPt(0), 
    summary(0), eventCount(0), minY(0), maxY(0),
    fTracklet(0), mp(0), fDigMan(0), fGeo(0),
    fDigitsInputFileName("TRD.FltDigits.root"),
    fDigitsInputFile(0), fEventNoInFile(0),
    fOutputPath("/mnt/jsroot/data.min.js")
{
    // constructor
    DefineInput(0, TChain::Class()); // define the input of the analysis: in this case we take a 'chain' of events
                                     // this chain is created by the analysis manager, so no need to worry about it,
                                     // it does its work automatically
    DefineOutput(1, TList::Class()); // define the ouptut of the analysis: in this case it's a list of histograms
                                     // you can add more output objects by calling DefineOutput(2, classname::Class())
                                     // if you add more output objects, make sure to call PostData for all of them, and to
                                     // make changes to your AddTask macro!

    // create the digits manager
    fDigMan = new AliTRDdigitsManager;
    fDigMan->CreateArrays();

    // the geometry could be created in the constructor or similar
    fGeo = new AliTRDgeometry;
    if (! fGeo) {
        AliFatal("cannot create geometry ");
    }
}
//_____________________________________________________________________________
AliAnalysisTaskMyTask::~AliAnalysisTaskMyTask()
{
    // destructor
    if (fOutputList)
    {
        delete fOutputList; // at the end of your task, it is deleted from memory by calling this function
    }

    delete summary;
}

//_____________________________________________________________________________
void AliAnalysisTaskMyTask::UserCreateOutputObjects()
{
    // create output objects
    //
    // this function is called ONCE at the start of your analysis (RUNTIME)
    // here you ceate the histograms that you want to use
    //
    fOutputList = new TList();    // this is a list which will contain all of your histograms
                                  // at the end of the analysis, the contents of this list are written
                                  // to the output file
    fOutputList->SetOwner(kTRUE); // memory stuff: the list is owner of all objects it contains and will delete them
                                  // if requested (dont worry about this now)

    fHistPt = new TH2F("fHistPt", "fHistPt", 100, -90, 90, 6, 0, 6);       // create your histogra
    fOutputList->Add(fHistPt);          // don't forget to add it to the list! the list will be written to file, so if you want
                                        // your histogram in the output file, add it to the list!

    fTracklet = new TNtuple("fTracklet", "fTracklet", "x:y:z:ly");
    fOutputList->Add(fTracklet);

    cout << "Output Path: " << fOutputPath << endl;
                                        
    summary = new ofstream();
    summary->open(fOutputPath);
    *summary << "function getData() {\n" + TAB + "return [";

    PostData(1, fOutputList); // postdata will notify the analysis manager of changes / updates to the
                              // fOutputList object. the manager will in the end take care of writing your output to file
                              // so it needs to know what's in the output
}

//_____________________________________________________________________________
TFile* AliAnalysisTaskMyTask::OpenDigitsFile(TString inputfile,
					   TString digfile,
					   TString opt)
{
  // we should check if we are reading ESDs or AODs - for now, only
  // ESDs are supported

  if (digfile == "") {
    return NULL;
  }

  // construct the name of the digits file from the input file
  inputfile.ReplaceAll("AliESDs.root", digfile);

  // open the file
  AliInfo( "opening digits file " + inputfile
	   + " with option \"" + opt + "\"");
  TFile* dfile = new TFile(inputfile, opt);
  if (!dfile) {
    AliWarning("digits file '" + inputfile + "' cannot be opened");
  }

  return dfile;
}

//_____________________________________________________________________________
Bool_t AliAnalysisTaskMyTask::UserNotify()
{
  delete fDigitsInputFile;

  AliESDInputHandler *esdH = dynamic_cast<AliESDInputHandler*>
    (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());

  if ( ! esdH ) return kFALSE;
  if ( ! esdH->GetTree() ) return kFALSE;
  if ( ! esdH->GetTree()->GetCurrentFile() ) return kFALSE;

  TString fname = esdH->GetTree()->GetCurrentFile()->GetName();

  fDigitsInputFile = OpenDigitsFile(fname,fDigitsInputFileName,"");

  fEventNoInFile = -1;

  return kTRUE;
}

//_____________________________________________________________________________
// Print a single AliESDTrdTracklet
void AliAnalysisTaskMyTask::PrintTrdTracklet(AliESDTrdTracklet *tracklet, std::string indent)
{
    *summary << indent << "{" << endl;
    *summary << indent + TAB << "\"id\": \"E" << eventCount << "_L" << mp->at(tracklet) << "\"," << endl;
    *summary << indent + TAB << "\"stack\": " << AliTRDgeometry::GetStack(tracklet->GetDetector()) << "," << endl;
    *summary << indent + TAB << "\"sector\": " << AliTRDgeometry::GetSector(tracklet->GetDetector()) << "," << endl;
    *summary << indent + TAB << "\"layer\": " << AliTRDgeometry::GetLayer(tracklet->GetDetector()) << "," << endl;
    *summary << indent + TAB << "\"hc\": " << tracklet->GetHCId() % 2 << "," << endl;
    *summary << indent + TAB << "\"binDy\": " << tracklet->GetBinDy() << "," << endl;
    *summary << indent + TAB << "\"binY\": " << tracklet->GetBinY() << "," << endl;
    *summary << indent + TAB << "\"binZ\": " << tracklet->GetBinZ() << "," << endl;
    *summary << indent + TAB << "\"dyDx\": " << tracklet->GetDyDx() << "," << endl;
    *summary << indent + TAB << "\"localY\": " << tracklet->GetLocalY() << "," << endl;
    *summary << indent << "}";    
}

//_____________________________________________________________________________
// Print the array of AliESDTrdTracklet, calling PrintTrdTracklet for each
void AliAnalysisTaskMyTask::PrintTrdTrackletArray(AliESDEvent *fESD, std::string indent)
{
    Int_t nTRDTracklets(fESD->GetNumberOfTrdTracklets());

    *summary << indent << "\"trdTracklets\": [" << endl;
    for (Int_t idx = 0; idx < nTRDTracklets; idx++)
    {
        AliESDTrdTracklet *tracklet = fESD->GetTrdTracklet(idx);
        PrintTrdTracklet(tracklet, indent + TAB);
        *summary << (idx + 1 == nTRDTracklets ? "" : ",") << endl;
    }
    *summary << indent << "]";
}

//_____________________________________________________________________________
// Print the array of AliESDTrdTrack
void AliAnalysisTaskMyTask::PrintTrdTrackArray(AliESDEvent *fESD, std::string indent)
{
    std::map<AliESDtrack *, AliESDTrdTrack *> trdTrackMap;
    Int_t nTrdTracks(fESD->GetNumberOfTrdTracks());

    for (Int_t trdIdx = 0; trdIdx < nTrdTracks; trdIdx++) {
        AliESDTrdTrack * trdTrack = fESD->GetTrdTrack(trdIdx);
        AliVTrack *trackMatch = trdTrack->GetTrackMatch();
        if (trackMatch != nullptr)
        {
            trdTrackMap.insert({(AliESDtrack *)trackMatch, trdTrack});
        }
    }

    Int_t nTracks(fESD->GetNumberOfTracks());

    *summary << indent << "\"trdTracks\": [" << endl;
    for (Int_t idx = 0; idx < nTracks; idx++)
    {
        AliESDtrack *track = fESD->GetTrack(idx);

        const AliExternalTrackParam * param = track->GetOuterParam();
        if (!param) continue;
        
        Float_t alpha = param->GetAlpha();
        if (alpha < 0) alpha += TMath::Pi() * 2;

        Float_t tanLambda = param->GetParameter()[3]; // tangent of the track momentum dip angle
        Float_t lambdaDeg = TMath::ATan(tanLambda) * 180 / TMath::Pi();

        Int_t sector = TMath::Nint(18.0 * alpha / (2 * TMath::Pi()) - 0.5);        
        Int_t stack = -1;
        if (lambdaDeg > 30) stack = 0;
        else if (lambdaDeg > 8) stack = 1;
        else if (lambdaDeg > -8) stack = 2;
        else if (lambdaDeg > -30) stack = 3;
        else stack = 4;

        Bool_t isTrd = false;

        if (trdTrackMap.count(track) == 1) {
            AliESDTrdTrack * trdTrack = trdTrackMap.at(track);
            sector = trdTrack->GetSector();
            stack = trdTrack->GetStack();
            isTrd = true;
        }
        
        *summary << indent + TAB << "{" << endl;
        *summary << indent + TAB + TAB << "\"id\": \"E" << eventCount << "_T" << idx << "\"," << endl;
        *summary << indent + TAB + TAB << "\"alpha\": " << alpha << "," << endl;
        *summary << indent + TAB + TAB << "\"lambda\": " << lambdaDeg << "," << endl;
        *summary << indent + TAB + TAB << "\"stack\": " << stack << "," << endl;
        *summary << indent + TAB + TAB << "\"sector\": " << sector << "," << endl;
        *summary << indent + TAB + TAB << "\"pT\": " << track->Pt() << "," << endl;
        *summary << indent + TAB + TAB << "\"type\": \"" << (isTrd ? "Trd Track" : "Esd Track") << "\"," << endl;
        
        PrintEsdTrack(track, indent + TAB + TAB);
            *summary << "," << endl;

        *summary << indent + TAB + TAB << "\"trdTracklets\": [";
        if (trdTrackMap.count(track) == 1) {
            AliESDTrdTrack * trdTrack = trdTrackMap.at(track);

            bool first = true;
            for (Int_t layerIndex = 0; layerIndex < 6; layerIndex++)
            {
                AliESDTrdTracklet *tracklet = trdTrack->GetTracklet(layerIndex);
                if (tracklet != nullptr)
                {
                    *summary << (first ? "" : ",") << endl;
                    first = false;
                    PrintTrdTracklet(tracklet, indent + TAB + TAB + TAB);
                }
            }
        }
        
        *summary << endl;
        *summary << indent + TAB + TAB << "]" << endl;
        
        *summary << indent + TAB << "}" << (idx + 1 == nTracks ? "" : ",") << endl;
    }
    
    *summary << indent << "]";
}

//_____________________________________________________________________________
// Print a single AliESDtrack as an array of [x, y, z] space points
void AliAnalysisTaskMyTask::PrintEsdTrack(AliESDtrack *track, std::string indent)
{
    *summary << indent << "\"track\": {" << endl;
    
    *summary << indent + TAB << "\"path\": [" << endl << indent + TAB + TAB ;
    
    Double_t b = track->GetESDEvent()->GetMagneticField();
    
    Double_t * xyz = new Double_t[3];
    for (Int_t x = 1; x <= 470; x+=10)
        if (track->GetXYZAt(x, b, xyz))
            *summary << "{ \"x\": " << xyz[0] << ", \"y\": " << xyz[1] << ", \"z\": " << xyz[2] << "},";
    delete [] xyz;

    *summary  << endl << indent + TAB << "]," << endl;

    *summary << indent << "}";
}

//_____________________________________________________________________________
void AliAnalysisTaskMyTask::UserExec(Option_t *)
{
    fESD = dynamic_cast<AliESDEvent *>(InputEvent());
    const AliESDVertex * primaryVertex = fESD->GetPrimaryVertex();

    fEventNoInFile = fESD->GetEventNumberInFile();

    if (!fESD)
        return; // if the pointer to the event is empty (getting it failed) skip this event

    if (eventCount > 5) 
            return; // Only take the first 5 valid events

    // if (fESD->GetEventNumberInFile() != 17) return;

    if (fESD->GetNumberOfTrdTracks() == 0)
        return; // Only interested in events with at least 1 TRD track

    if (!ReadDigits()) 
            return; // If there are no digits for this event, skip

            if (++eventCount > 1) 
                *summary << "," << endl;
            else *summary << endl;

            std::string indent = TAB + TAB;

            *summary << indent << "{" << endl
                    << indent + TAB << "\"evno\": " << fESD->GetEventNumberInFile() << "," << endl
                    << indent + TAB << "\"id\": \"E" << eventCount << "\"," << endl;

            Int_t nTRDTracklets(fESD->GetNumberOfTrdTracklets());

    // Create a map from tracklet pointers to corresponding index
            mp = new map<AliESDTrdTracklet *, Int_t>;

            for (Int_t idx = 0; idx < nTRDTracklets; idx++)
            {    
                AliESDTrdTracklet *tracklet = fESD->GetTrdTracklet(idx);
                mp->insert({tracklet, idx});
            }

            PrintTrdTrackArray(fESD, indent + TAB);
            *summary << "," << endl;
            PrintTrdTrackletArray(fESD, indent + TAB);
            *summary << endl;

            delete mp;

            *summary << indent << "}";   

    PostData(1, fOutputList); // stream the results the analysis of this event to
                              // the output manager which will take care of writing
                              // it to a file
}

//________________________________________________________________________
Bool_t AliAnalysisTaskMyTask::ReadDigits()
{

  if (!fDigMan) {
    AliError("no digits manager");
    return false;
  }

  if (!fDigitsInputFile) {
    AliError("digits file not available");
    return false;
  }

  // read digits from file
  TTree* tr = (TTree*)fDigitsInputFile->Get(Form("Event%d/TreeD",
                                                 fEventNoInFile));

  if (!fDigitsInputFile) {
    AliError(Form("digits tree for event %d not found", fEventNoInFile));
    return false;
  }

  if (!tr) {
    AliError("error with digits tree");
    return false;
  }

  // reset digit arrays
  for (Int_t det=0; det<540; det++) {
    fDigMan->ClearArrays(det);
    fDigMan->ClearIndexes(det);
  }

  fDigMan->ReadDigits(tr);
  delete tr;

  // expand digits for use in this task
  for (Int_t det=0; det<540; det += 6) {
    if (fDigMan->GetDigits(det)) {
        //cout << "Creating file: " << Form("/mnt/jsroot/data/%d.%d.json", fEventNoInFile, det) << endl;
        Int_t sector = fGeo->GetSector(det), stack = fGeo->GetStack(det), layer = fGeo->GetLayer(det);

        ofstream dout(Form("/mnt/jsroot/data/%d.%d.%d.json", fEventNoInFile, sector, stack), std::ofstream::out | std::ofstream::trunc);
        dout << "{\n\t\"event\": " << fEventNoInFile << ",\n"
             << "\t\"sector\": " << sector << ",\n"
             << "\t\"stack\": " << stack << ",\n"
             << "\t\"layers\": [\n";

        for (Int_t layer = 0; layer < 6; layer++) {

            dout << "\t\t{\n"
                 << "\t\t\t\"event\": " << fEventNoInFile << ",\n"
                 << "\t\t\t\"sector\": " << sector << ",\n"
                 << "\t\t\t\"stack\": " << stack << ",\n"<< "\t\t\t\"layer\": " << layer << ",\n"
                 << "\t\t\t\"det\": " << det + layer << ",\n"
                 << "\t\t\t\"pads\": [\n";

            AliTRDarrayADC * adcArray = fDigMan->GetDigits(det + layer);
            adcArray->Expand();

            Int_t nrow = adcArray->GetNrow(), ncol = adcArray->GetNcol(), ntime = adcArray->GetNtime();
            Int_t maxTsum = 0;

            for (Int_t r = 0; r < nrow; r++) {
                for (Int_t c = 0; c < ncol; c++) {
                    dout << "\t\t\t\t{\n"
                        << "\t\t\t\t\t\"row\": " << r << ",\n"
                        << "\t\t\t\t\t\"col\": " << c << ",\n"
                        << "\t\t\t\t\t\"layer\": " << layer << ",\n"
                        << "\t\t\t\t\t\"tbins\": [";

                    Int_t tsum = 0;
                    Short_t data;
                    for (Int_t t = 0; t < ntime; t++) {
                        data = adcArray->GetData(r, c, t);
                        tsum += data;
                        dout << data << ((t + 1 < ntime) ? ", " : "");
                    }

                    dout << "],\n";
                    dout << "\t\t\t\t\t\"tsum\": " << tsum << "\n";
                    dout << "\t\t\t\t}" << ((c + 1 < ncol) || (r + 1 < nrow) ? "," : "") << endl;

                    if (tsum > maxTsum) maxTsum = tsum;
                }
            }

            dout << "\t\t\t],\n";
            dout << "\t\t\t\"maxtsum\": " << maxTsum << "\n";
            dout << "\t\t}" << (layer + 1 < 6 ? "," : "") << endl;
        }

        dout << "\t]\n}";

        dout.flush();
        dout.close();
    }
    else {
        cout << "Could not get digits" << endl;
    }
  }
  
  return kTRUE;
}

//_____________________________________________________________________________
void AliAnalysisTaskMyTask::Terminate(Option_t *)
{
    // terminate
    // called at the END of the analysis (when all events are processed)
    *summary << endl;
    *summary << TAB << "];" << endl 
            << "}" << endl;
    summary->flush();
    summary->close();
}