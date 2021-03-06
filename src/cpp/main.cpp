#include <vector>

#include "includes.hpp"
#include "util.hpp"
#include "ymt.hpp"

using std::vector;
using std::string;

enum Action {
    STATUS,
    LOAD,
    UNLOAD,
	EXPORT_LOG,
	EXPORT_STAT,
    UNKNOWN
};

int main(int argc, char*const argv[]) {

    //Variables
    string input_file;
	string out_file = "yamato_analyzed.log";
	string input_log_file;

    Action action = UNKNOWN;
    string port;

#pragma region Collect parameters
    for(int a = 1; a < argc; ++a) {

        if (util::Search(argv[a], {"-i", "--input"}, true) != -1) {
            if (a + 1 < argc) input_file = argv[a + 1];
            else {
                util::ReportError("You need to specify filename when using -i or --input.");
                exit(0);
            }
            a += 1;
        }

        else if (util::Search(argv[a], {"-a", "--action"}, true) != -1) {

            //Parameter pre-check
            if (a + 1 >= argc) {
                util::ReportError("You need to specify action when using -a or --action.");
                break;
            }

            if (util::Search(argv[a + 1], {"status"}, true) != -1) {
                action = STATUS;
            } else if (util::Search(argv[a + 1], {"load"}, true) != -1) {
                action = LOAD;
            } else if (util::Search(argv[a + 1], {"unload"}, true) != -1) {
                action = UNLOAD;
			}
			else if (util::Search(argv[a + 1], { "export-log" }, true) != -1 || util::Search(argv[a + 1], { "el" }, true) != -1) {
				action = EXPORT_LOG;
			}
			else if (util::Search(argv[a + 1], { "export-stat" }, true) != -1 || util::Search(argv[a + 1], { "el" }, true) != -1) {
				action = EXPORT_STAT;
			}
			else {
                util::ReportError("Unknown action: " + string(argv[a + 1]) + ".");
                break;
            }
            a += 1;
        }

        else if (util::Search(argv[a], {"/?", "-h", "--help"}, true) != -1) {
            printf("YAMATO " SOFTWARE_VERSION " by LBYPatrick\n");
            util::ShowHelp({
                                   {"-i or --input <filename>",        "specify input file"},
                                   {"-a or --action <action>",         "specify action (status, load, unload)"},
                                   {"/?, -h or --help",                "show this help message"},
                                   {"-e or --extra-parameter <param>", "specify additional parameters, you can do things like UDP relay or HTTP/TLS OBFS here"},
                                   {"-p or --port",                    "specify a port for checking status"}

                           });
            printf("\n");
            return 0;
        }

		else if (util::Search(argv[a], { "-e", "--extra-parameter" }, true) != -1) {

			if (a + 1 >= argc) {
				util::ReportError("Your need to specify extra parameters (With quote if needed) when using -e or --extra-parameter.");
				return 0;
			}

			ymt::SetExtraParam(argv[a + 1]);
			a += 1;
		}

		else if (util::Search(argv[a], { "-o","--output" }, true) != -1) {
			if (a + 1 >= argc) {
				util::ReportError("You need to specify a filename when using -o or --output.");
				return 0;
			}
			
			out_file = argv[a + 1];

			a += 1;
		}

		else if (util::Search(argv[a], { "-li","--log-input" }, true) != -1) {
			if (a + 1 >= argc) {
				util::ReportError("You need to specify a filename when using -li or --log-input.");
				return 0;
			}

			input_log_file = argv[a + 1];
			ymt::SetAttribute(LOG_INPUT_FILENAME, input_log_file);
			
			a += 1;
		}

        else if (util::Search(argv[a], {"-p","--port"},true) != -1) {

            if(a+1 >= argc) {
                util::ReportError("You need to specify a port when using -p");
                return 0;
            }

            port = argv[a+1];

            a += 1;
        }

        else {
            util::ReportError("Unknown option: " + string(argv[a]) + ".");
            return 0;
        }
    }
#pragma endregion

#pragma region parameter check & execute
    //Finish collecting parameters, run pre-check
    if (action == UNKNOWN) {
        util::ReportError("Need to specify action with -a or --action.");
		return 0;
    }

	//Set Input File Name
	ymt::SetFileName(input_file);

    //Start Execution
    switch(action) {
        case STATUS :

            if(port.empty()) {
                util::ReportError("You need to specify a port for checking status");
				return 0;
            }
            if(input_file.empty()) {
                vector<string> file_list = util::GetFileList("./");

                for(string & file : file_list) {
                    if(file.find(".pidmap") != -1) {
                        input_file = util::SubString(file,0,file.size()-7);
                        break;
                    }
                }

                if(input_file.empty()) {
                    util::ReportError("Need to specify action with -a or --action.");
                    exit(0);
                }

				ymt::SetFileName(input_file);
            }
            ymt::CheckPort(port);
            break;

        case LOAD :
            if (input_file.empty()) {
                util::ReportError("Need to specify input file with -i or --input.");
				return 0;
            }
            ymt::RunConfig();
            break;

        case UNLOAD :
            if (input_file.empty()) {
                util::ReportError("Need to specify input file with -i or --input.");
				return 0;
            }
            ymt::StopConfig();
            break;
		case EXPORT_LOG :
			util::WriteFile(out_file, ymt::GetFormattedStringData());
			printf("Log saved to \"%s\".\n",out_file.c_str());
			break;
		case EXPORT_STAT :
			util::WriteFile(out_file, ymt::GetStatisics());
			printf("Statistics saved to \"%s\".\n", out_file.c_str());
			break;
    }

	return 1;
#pragma endregion

}