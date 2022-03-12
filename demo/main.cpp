///////////////////////////////////////////////////
// Low Level Parallel Programming 2017.
//
//
//
// The main starting point for the crowd simulation.
//

#undef max
#include "MainWindow.h"
#include "ParseScenario.h"
#include "ped_model.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTimer>
#include <thread>

#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include "PedSimulation.h"

#pragma comment(lib, "libpedsim.lib")

#include <stdlib.h>

int main(int argc, char* argv[]) {
    bool timing_mode = 0;
    int i = 1;
    int threadNum = 0;
    Ped::IMPLEMENTATION impl = Ped::IMPLEMENTATION::SEQ;
    QString scenefile = "scenario.xml";

    // Argument handling
    while (i < argc) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            if (strcmp(&argv[i][2], "timing-mode") == 0) {
                cout << "Timing mode on\n";
                timing_mode = true;
            } else if (strcmp(&argv[i][2], "help") == 0) {
                cout << "Usage: " << argv[0]
                     << " [--help] [--timing-mode] [--threads NUM] [--impl "
                        "IMPL][scenario]"
                     << endl;
                return 0;
            } else if (strcmp(&argv[i][2], "threads") == 0) {
                i = i + 1;
                threadNum = strtol(argv[i], NULL, 10);
                cout << "thread " << threadNum << endl;
            } else if (strcmp(&argv[i][2], "impl") == 0) {
                i = i + 1;
                if (strcmp(argv[i], "OMP") == 0) {
                    impl = Ped::IMPLEMENTATION::OMP;
                } else if (strcmp(argv[i], "PTHREAD") == 0) {
                    impl = Ped::IMPLEMENTATION::PTHREAD;
                } else if (strcmp(argv[i], "VECTOR") == 0) {
                    impl = Ped::IMPLEMENTATION::VECTOR;
                } else if (strcmp(argv[i], "CUDA") == 0) {
                    impl = Ped::IMPLEMENTATION::CUDA;
                } else if (strcmp(argv[i], "REGION") == 0) {
                    impl = Ped::IMPLEMENTATION::REGION;
                } else {
                    cerr << "Unrecognized implementation " << endl;
                    return 0;
                }
                cout << "impl " << impl << endl;
            } else {
                cerr << "Unrecognized command: \"" << argv[i]
                     << "\". Ignoring ..." << endl;
                return 0;
            }
        } else  // Assume it is a path to scenefile
        {
            scenefile = argv[i];
        }

        i += 1;
    }

    int retval = 0;
    {  // This scope is for the purpose of removing false memory leak positives

        // Reading the scenario file and setting up the crowd simulation model
        Ped::Model model;
        ParseScenario parser(scenefile);
        model.setup(parser.getAgents(), parser.getWaypoints(), impl);

        // Default number of steps to simulate. Feel free to change this.
        const int maxNumberOfStepsToSimulate = 10;

        // Timing version
        // Run twice, without the gui, to compare the runtimes.
        // Compile with timing-release to enable this automatically.
        if (timing_mode) {
            // MainWindow mainwindow(model, timing_mode);
            //  Run sequentially

            double fps_seq, fps_target;
			double seqCreationAvg, seqScalingAvg, seqFilterAvg;
			double cudaCreationAvg, cudaScalingAvg, cudaFilterAvg;
            {
                Ped::Model model;
                ParseScenario parser(scenefile);
                model.setup(parser.getAgents(), parser.getWaypoints(),
                            Ped::SEQ);
                PedSimulation simulation(model, NULL, timing_mode);
                // Simulation mode to use when profiling (without any GUI)
                std::cout << "Running reference version...\n";
                auto start = std::chrono::steady_clock::now();
                simulation.runSimulation(maxNumberOfStepsToSimulate);
                auto duration_seq =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start);
                fps_seq = ((float)simulation.getTickCount()) /
                          ((float)duration_seq.count()) * 1000.0;
                cout << "Reference time: " << duration_seq.count()
                     << " milliseconds, " << fps_seq << " Frames Per Second."
                     << std::endl;
				
				seqCreationAvg =
                    model.hmCreationSeqTotal / maxNumberOfStepsToSimulate;
                seqScalingAvg = model.hmScalingSeqTotal / maxNumberOfStepsToSimulate;
                seqFilterAvg = model.hmFilterSeqTotal / maxNumberOfStepsToSimulate;

            }

            // Change this variable when testing different versions of your
            // code. May need modification or extension in later assignments
            // depending on your implementations
            Ped::IMPLEMENTATION implementation_to_test = impl;
            {
                Ped::Model model;
                ParseScenario parser(scenefile);
                model.setup(parser.getAgents(), parser.getWaypoints(),
                            implementation_to_test);
                model.setThreadNum(threadNum);
                PedSimulation simulation(model, NULL, timing_mode);
                // Simulation mode to use when profiling (without any GUI)
                std::cout << "Running target version...\n";
                auto start = std::chrono::steady_clock::now();
                simulation.runSimulation(maxNumberOfStepsToSimulate);
                auto duration_target =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start);
                fps_target = ((float)simulation.getTickCount()) /
                             ((float)duration_target.count()) * 1000.0;
                cout << "Target time: " << duration_target.count()
                     << " milliseconds, " << fps_target << " Frames Per Second."
                     << std::endl;
				cudaCreationAvg =
                    model.hmCreationCUDATotal / maxNumberOfStepsToSimulate;
                cudaScalingAvg =
                    model.hmScalingCUDATotal / maxNumberOfStepsToSimulate;
                cudaFilterAvg =
                    model.hmFilterCUDATotal / maxNumberOfStepsToSimulate;
            }
            std::cout << "\n\nSpeedup: " << fps_target / fps_seq << std::endl;

            if (implementation_to_test == Ped::IMPLEMENTATION::REGION) {
                // double seqCreationAvg =
                //     model.hmCreationSeqTotal / maxNumberOfStepsToSimulate;
                // double seqScalingAvg = model.hmScalingSeqTotal / maxNumberOfStepsToSimulate;
                // double seqFilterAvg = model.hmFilterSeqTotal / maxNumberOfStepsToSimulate;

                // double cudaCreationAvg =
                //     model.hmCreationCUDATotal / maxNumberOfStepsToSimulate;
                // double cudaScalingAvg =
                //     model.hmScalingCUDATotal / maxNumberOfStepsToSimulate;
                // double cudaFilterAvg =
                //     model.hmFilterCUDATotal / maxNumberOfStepsToSimulate;

                double creationSpeedup = seqCreationAvg / cudaCreationAvg;
                double scalingSpeedup = seqCreationAvg / cudaScalingAvg ;
                double filterSpeedup = seqFilterAvg / cudaFilterAvg;
				printf("%lf \n", model.hmFilterSeqTotal);

                printf("[Result]\n");
                printf(
                    "\t[Sequntial Average] Creation: %lfms, Scaling: %lfms, "
                    "filter: %lfms\n",
                    seqCreationAvg, seqScalingAvg, seqFilterAvg);
                printf(
                    "\t[  CUDA Average   ] Creation: %lfms, Scaling: %lfms, filter: "
                    "%lfms\n",
                    cudaCreationAvg, cudaScalingAvg, cudaFilterAvg);
                printf(
					"\t[     Speedup     ] Creation: %lf, Scaling: %lf, filter: %lf\n",
                       creationSpeedup, scalingSpeedup, filterSpeedup);
            }

        }
        // Graphics version
        else {
            QApplication app(argc, argv);
            MainWindow mainwindow(model);

            PedSimulation simulation(model, &mainwindow, timing_mode);

            cout << "Demo setup complete, running ..." << endl;

            // Simulation mode to use when visualizing
            auto start = std::chrono::steady_clock::now();
            mainwindow.show();
            simulation.runSimulation(maxNumberOfStepsToSimulate);
            retval = app.exec();

            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);
            float fps = ((float)simulation.getTickCount()) /
                        ((float)duration.count()) * 1000.0;
            cout << "Time: " << duration.count() << " milliseconds, " << fps
                 << " Frames Per Second." << std::endl;
        }
    }

    cout << "Done" << endl;
    cout << "Type Enter to quit.." << endl;
    getchar();  // Wait for any key. Windows convenience...
    return retval;
}