#include "catchintons.h"
#include "readfunc.h"
#include "readword.h"
#include "readaggregation.h"
#include "errorhandler.h"
#include "areatime.h"
#include "fleet.h"
#include "stock.h"
#include "mortpredlength.h"
#include "ecosystem.h"
#include "gadget.h"

CatchInTons::CatchInTons(CommentStream& infile, const AreaClass* const areainfo,
  const TimeClass* const timeinfo, double likweight) : Likelihood(CATCHINTONSLIKELIHOOD, likweight) {

  ErrorHandler handle;
  int i, j;
  int numarea = 0;
  char text[MaxStrLength];
  char datafilename[MaxStrLength];
  char aggfilename[MaxStrLength];

  strncpy(text, "", MaxStrLength);
  strncpy(datafilename, "", MaxStrLength);
  strncpy(aggfilename, "", MaxStrLength);
  ifstream datafile;
  CommentStream subdata(datafile);

  functionname = new char[MaxStrLength];
  strncpy(functionname, "", MaxStrLength);
  ReadWordAndValue(infile, "datafile", datafilename);
  ReadWordAndValue(infile, "function", functionname);

  debug_print = 0;
  functionnumber = 0;
  if (strcasecmp(functionname, "sumofsquares") == 0) {
    functionnumber = 1;
  } else if (strcasecmp(functionname, "sumofsquares-debug") == 0) {
    functionnumber = 1;
    debug_print = 1;
  } else
    handle.Message("Unknown function in catchintons");

  infile >> ws;
  if (infile.peek() == 'a') {
    infile >> text >> ws;
    if (strcasecmp(text, "aggregationlevel") == 0)
      infile >> yearly >> ws;
    else
      handle.Unexpected("aggregationlevel", text);

  } else
    yearly = 0;

  if (yearly != 0 && yearly != 1)
    handle.Message("Error in catchintons - aggregationlevel must be 0 or 1");

  ReadWordAndVariable(infile, "epsilon", epsilon);
  if (epsilon <= 0) {
    handle.Warning("Epsilon should be a positive number - set to default value 10");
    epsilon = 10;
  }

  ReadWordAndValue(infile, "areaaggfile", aggfilename);
  datafile.open(aggfilename);
  CheckIfFailure(datafile, aggfilename);
  handle.Open(aggfilename);
  numarea = ReadAggregation(subdata, areas, areaindex);
  handle.Close();
  datafile.close();
  datafile.clear();

  if (debug_print) {
    for (i = 0; i < areaindex.Size(); i++)
      cout << areaindex[i] << endl;
    for (i = 0; i < areas.Nrow(); i++) {
      for (j = 0; j < areas.Ncol(i); j++)
        cout << areas[i][j] << sep;
      cout << endl;
    }
  }

  //Must change from outer areas to inner areas.
  for (i = 0; i < areas.Nrow(); i++)
    for (j = 0; j < areas.Ncol(i); j++)
      if ((areas[i][j] = areainfo->InnerArea(areas[i][j])) == -1)
        handle.UndefinedArea(areas[i][j]);

  //Read in the fleetnames
  i = 0;
  infile >> text >> ws;
  if (!(strcasecmp(text, "fleetnames") == 0))
    handle.Unexpected("fleetnames", text);
  infile >> text >> ws;
  while (!infile.eof() && !(strcasecmp(text, "stocknames") == 0)) {
    fleetnames.resize(1);
    fleetnames[i] = new char[strlen(text) + 1];
    strcpy(fleetnames[i++], text);
    infile >> text >> ws;
  }

  //Read in the stocknames
  i = 0;
  if (!(strcasecmp(text, "stocknames") == 0))
    handle.Unexpected("stocknames", text);
  infile >> text;
  while (!infile.eof() && !(strcasecmp(text, "[component]") == 0)) {
    stocknames.resize(1);
    stocknames[i] = new char[strlen(text) + 1];
    strcpy(stocknames[i++], text);
    infile >> text >> ws;
  }

  if (debug_print) {
    for (i = 0; i < stocknames.Size(); i++)
      cout << stocknames[i] << endl;
    for (i = 0; i < fleetnames.Size(); i++)
      cout << fleetnames[i] << endl;
  }

  //We have now read in all the data from the main likelihood file
  //But we have to read in the statistics data from datafilename
  datafile.open(datafilename);
  CheckIfFailure(datafile, datafilename);
  handle.Open(datafilename);
  ReadCatchInTonsData(subdata, timeinfo, numarea);
  handle.Close();
  datafile.close();
  datafile.clear();
}

void CatchInTons::Reset(const Keeper* const keeper) {
  Likelihood::Reset(keeper);
  timeindex = 0;
  int i, j;
  for (i = 0; i < ModelCatch.Nrow(); i++)
    for (j = 0; j < ModelCatch.Ncol(i); j++)
      ModelCatch[i][j] = 0.0;
}

void CatchInTons::Print(ofstream& outfile) const {
  int i;

  outfile << "\nCatch in Tons\nlikelihood " << likelihood
    << "\nfunction " << functionname;
  outfile << "\n\tStocknames:";
  for (i = 0; i < stocknames.Size(); i++)
    outfile << sep << stocknames[i];
  outfile << "\n\tFleetnames:";
  for (i = 0; i < fleetnames.Size(); i++)
    outfile << sep << fleetnames[i];
  outfile << endl;
  outfile.flush();
}

double CatchInTons::SumOfSquares(double obs, double mod) {
  double t = log(obs + epsilon) - log(mod + epsilon);
  return t * t;
}

void CatchInTons::AddToLikelihood(const TimeClass* const TimeInfo) {
  int a, a2, f, p;
  double t = 0.0;
  PopPredator* pred;
  assert(preyindex.Nrow() == fleets.Size());

  if (AAT.AtCurrentTime(TimeInfo)) {
    for (a = 0; a < areas.Nrow(); a++) {
      for (a2 = 0; a2 < areas.Ncol(a); a2++) {
        for (f = 0; f < preyindex.Nrow(); f++) {
          pred = (PopPredator*)fleets[f]->ReturnPredator();
          for (p = 0; p < preyindex.Ncol(f); p++)
            ModelCatch[timeindex][a] += pred->consumedBiomass(preyindex[f][p], a2);
        }
      }

      if (!yearly) {
        switch(functionnumber) {
          case 1:
          t = SumOfSquares(ModelCatch[timeindex][a], DataCatch[timeindex][a]);
          break;
        default:
          cerr << "Error in catchintons - unknown function " << functionname << endl;
          break;
        }

        likelihood += t;
        if (debug_print) {
          cout << TimeInfo->CurrentYear() << sep << TimeInfo->CurrentStep() << sep
            << DataCatch[timeindex][a] << sep << ModelCatch[timeindex][a] << sep << t << endl;
        }

      } else if (TimeInfo->CurrentStep() == TimeInfo->StepsInYear()) {
        switch(functionnumber) {
          case 1:
          t = SumOfSquares(ModelCatch[timeindex][a], DataCatch[timeindex][a]);
          break;
        default:
          cerr << "Error in catchintons - unknown function " << functionname << endl;
          break;
        }

        likelihood += t;
        if (debug_print) {
          cout << TimeInfo->CurrentYear() << sep << DataCatch[timeindex][a] << sep
            << ModelCatch[timeindex][a] << sep << t << endl;
        }
      }
    }

    if (!yearly)
      timeindex++;
    else if (TimeInfo->CurrentStep() == TimeInfo->StepsInYear())
      timeindex++;
  }
}

CatchInTons::~CatchInTons() {
  int i;
  for (i = 0; i < stocknames.Size(); i++)
    delete[] stocknames[i];
  for (i = 0; i < fleetnames.Size(); i++)
    delete[] fleetnames[i];
  for (i = 0; i < areaindex.Size(); i++)
    delete[] areaindex[i];
  delete[] functionname;
}

void CatchInTons::SetFleetsAndStocks(Fleetptrvector& Fleets, Stockptrvector& Stocks) {
  int i, j, k, found;

  for (i = 0; i < fleetnames.Size(); i++) {
    found = 0;
    for (j = 0; j < Fleets.Size(); j++)
      if (strcasecmp(fleetnames[i], Fleets[j]->Name()) == 0) {
        found = 1;
        fleets.resize(1, Fleets[j]);
      }

    if (found == 0) {
      cerr << "Error: when searching for names of fleets for CatchInTons.\n"
        << "Did not find any name matching " << fleetnames[i] << endl;
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < stocknames.Size(); i++) {
    found = 0;
    for (j = 0; j < Stocks.Size(); j++) {
      if (Stocks[j]->IsEaten())
        if (strcasecmp(stocknames[i], Stocks[j]->ReturnPrey()->Name()) == 0) {
          found = 1;
          stocks.resize(1, Stocks[j]);
        }
    }
    if (found == 0) {
      cerr << "Error: when searching for names of stocks for CatchInTons.\n"
        << "Did not find any name matching " << stocknames[i] << endl;
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < fleets.Size(); i++) {
    found = 0;
    preyindex.AddRows(1, 0);
    Predator* pred = fleets[i]->ReturnPredator();
    for (j = 0; j < pred->NoPreys(); j++)
      for (k = 0; k < stocknames.Size(); k++)
        if (strcasecmp(stocknames[k], pred->Preys(j)->Name()) == 0) {
          found = 1;
          preyindex[i].resize(1, j);
        }

    if (found == 0)
      cerr << "Warning: when searching for names of stocks and fleets for CatchInTons.\n"
        << "Fleet " << fleetnames[i] << " is not catching any of the included stocks\n";
  }

  if (debug_print) {
    cout << DataCatch.Nrow() << sep << Years.Size() << sep << Steps.Size() << endl;
    for (i = 0; i < DataCatch.Nrow(); i++) {
      cout << Years[i] << sep;
      if (!yearly)
        cout << Steps[i] << sep;
      cout << DataCatch[i][0] << endl;
    }
  }
}

void CatchInTons::ReadCatchInTonsData(CommentStream& infile,
  const TimeClass* TimeInfo, int numarea) {

  int i;
  int year, step;
  double tmpnumber = 0.0;
  char tmparea[MaxStrLength];
  char tmpfleet[MaxStrLength];
  strncpy(tmparea, "", MaxStrLength);
  strncpy(tmpfleet, "", MaxStrLength);
  int keepdata, timeid, areaid, fleetid, check;
  int count = 0;
  ErrorHandler handle;

  //Find start of distribution data in datafile
  infile >> ws;
  char c;
  c = infile.peek();
  if (!isdigit(c)) {
    infile.get(c);
    while (c != '\n' && !infile.eof())
      infile.get(c);
  }

  //Check the number of columns in the inputfile
  check = CountColumns(infile);
  if (!(((yearly == 1) && ((check == 4) || (check == 5))) || ((yearly == 0) && (check == 5))))
    handle.Message("Wrong number of columns in inputfile - should be 4 or 5");

  while (!infile.eof()) {
    keepdata = 0;
    if ((yearly == 1) && (check == 4))
      infile >> year >> tmparea >> tmpfleet >> tmpnumber >> ws;
    else
      infile >> year >> step >> tmparea >> tmpfleet >> tmpnumber >> ws;

    //check if the year and step are in the simulation
    timeid = -1;
    if (TimeInfo->IsWithinPeriod(year, step)) {
      //if this is a new timestep, resize to store the data
      for (i = 0; i < Years.Size(); i++)
        if ((Years[i] == year) && (yearly || (Steps[i] == step)))
          timeid = i;

      if (timeid == -1) {
        Years.resize(1, year);
        Steps.resize(1, step);
        timeid = (Years.Size() - 1);

        DataCatch.AddRows(1, numarea, 0.0);
        ModelCatch.AddRows(1, numarea, 0.0);
      }
    } else {
      //dont keep the data
      keepdata = 1;
    }

    //if tmparea is in areaindex find areaid, else dont keep the data
    areaid = -1;
    for (i = 0; i < areaindex.Size(); i++)
      if (strcasecmp(areaindex[i], tmparea) == 0)
        areaid = i;

    if (areaid == -1)
      keepdata = 1;

    //if tmpfleet is a required fleet keep the data, else ignore it
    fleetid = -1;
    for (i = 0; i < fleetnames.Size(); i++)
      if (!(strcasecmp(fleetnames[i], tmpfleet) == 0))
        fleetid = i;

    if (fleetid == -1)
      keepdata = 1;

    if (keepdata == 0) {
      //distribution data is required, so store it
      count++;
      //note that we use += to sum the data over all fleets (and possibly time)
      DataCatch[timeid][areaid] += tmpnumber;
    }
  }

  if (yearly)
    AAT.AddActionsAtAllSteps(Years, TimeInfo);
  else
    AAT.AddActions(Years, Steps, TimeInfo);

  if (count == 0)
    cout << "Warning in CatchInTons - found no data in the data file\n";
}