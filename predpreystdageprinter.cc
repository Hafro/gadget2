#include "predstdinfo.h"
#include "predpreystdprinter.h"
#include "areatime.h"
#include "stockpredstdinfo.h"
#include "gadget.h"

#include "runid.h"
extern RunID RUNID;

PredPreyStdAgePrinter::PredPreyStdAgePrinter(CommentStream& infile, const TimeClass* const TimeInfo)
  : PredPreyStdPrinter(infile, TimeInfo), predinfo(0), predator(0), prey(0) {

  //finished initializing. Now print first lines
  outfile << "; ";
  RUNID.print(outfile);
  outfile << "; Predation file by age for predator " << predname << " and prey " << preyname;

  if (printtimeid == 0)
    outfile << "\n; Printing the following information at the end of each timestep";
  else
    outfile << "\n; Printing the following information at the start of each timestep";

  outfile << "\n; year-step-area-pred age-prey age-cons number-cons biomass-mortality\n";
  outfile.flush();
}

PredPreyStdAgePrinter::~PredPreyStdAgePrinter() {
  delete predinfo;
}

void PredPreyStdAgePrinter::setPopPredAndPrey(const PopPredator* pred,
  const Prey* pRey, int IsStockPredator, int IsStockPrey) {

  predator = pred;
  prey = pRey;
  if (IsStockPredator) {
    if (IsStockPrey)
      predinfo = new StockPredStdInfo((const StockPredator*)(predator), (const StockPrey*)(prey), areas);
    else
      predinfo = new StockPredStdInfo((const StockPredator*)(predator), prey, areas);
  } else
    if (IsStockPrey)
      predinfo = new PredStdInfo(predator, (const StockPrey*)(prey), areas);
    else
      predinfo = new PredStdInfo(predator, prey, areas);
}

void PredPreyStdAgePrinter::Print(const TimeClass * const TimeInfo, int printtime) {

  if ((!AAT.atCurrentTime(TimeInfo)) || (printtime != printtimeid))
    return;

  int a, predage, preyage;

  for (a = 0; a < areas.Size(); a++) {
    predinfo->Sum(TimeInfo, areas[a]);
    for (predage = predinfo->NconsumptionByAge(areas[a]).minRow();
        predage <= predinfo->NconsumptionByAge(areas[a]).maxRow(); predage++) {
      for (preyage = predinfo->NconsumptionByAge(areas[a]).minCol(predage);
          preyage < predinfo->NconsumptionByAge(areas[a]).maxCol(predage); preyage++) {

        outfile << setw(lowwidth) << TimeInfo->getYear() << sep
          << setw(lowwidth) << TimeInfo->getStep() << sep
          << setw(lowwidth) << outerareas[a] << sep << setw(lowwidth)
          << predage << sep << setw(lowwidth) << preyage << sep;

        //JMB crude filter to remove the 'silly' values from the output
        if ((predinfo->NconsumptionByAge(areas[a])[predage][preyage] < rathersmall)
           || (predinfo->BconsumptionByAge(areas[a])[predage][preyage] < rathersmall)
           || (predinfo->MortalityByAge(areas[a])[predage][preyage] < verysmall))

          outfile << setw(width) << 0 << sep << setw(width) << 0 << sep << setw(width) << 0 << endl;

        else
          outfile << setprecision(precision) << setw(width)
            << predinfo->NconsumptionByAge(areas[a])[predage][preyage] << sep
            << setprecision(precision) << setw(width)
            << predinfo->BconsumptionByAge(areas[a])[predage][preyage] << sep
            << setprecision(precision) << setw(width)
            << predinfo->MortalityByAge(areas[a])[predage][preyage] << endl;

      }
    }
  }
  outfile.flush();
}
