
#include "ymt.hpp"
#include "util.hpp"

vector <string> log_buffer_;
string extra_param_;
string config_;
string input_log_;
vector <PIDInfo> pid_table_;
vector <Parser> users_;

void ymt::RunConfig() {

    vector <string> yaml_content;
    vector <PIDInfo> pids;

    Parser default_config;

    //Unload sessions from the config session if loaded already

    if (util::IsFileExist(config_ + ".pidmap")) StopConfig();

    yaml_content = util::ReadFile(config_);

    //Parse YAML
    for (string &line : yaml_content) {

        //Skip blanklines & comments
        if (line.empty()) continue;
        else {
            int slash_pos = line.find("//");
            if (slash_pos != -1) {
                line = util::SubString(line, 0, slash_pos);
            }
            if (line.empty()) continue;
        }


        YAML l = util::GetYaml(line);

        //If it is global level -- no user
        if (l.level == 0) {
            switch (util::Search(l.left, {"group", "nameserver", "method", "fastopen", "redirect", "timeout", "server",
                                          "tunnel_mode", "verbose"})) {
                case 0:
                    break; //Don't really know how can I use group name...
                case 1:
                    if (util::IsTheSame("localhost", l.right, false, false) ||
                        util::IsTheSame("bind9-local", l.right, false, false)) {
                        default_config.SetAttribute(DNS, util::GetMachineIP());
                    } else {
                        default_config.SetAttribute(DNS, l.right);
                    }
                    break;
                case 2:
                    default_config.SetAttribute(METHOD, l.right);
                    break;
                case 3:
                    default_config.SetAttribute(TCP_FASTOPEN, l.right);
                    break;
                case 4:
                    default_config.SetAttribute(REDIRECT, l.right);
                    break;
                case 5:
                    default_config.SetAttribute(TIMEOUT, l.right);
                    break;
                case 6:
                    default_config.SetAttribute(SERVER, l.right);
                    break;
                case 7:
                    default_config.SetAttribute(UDP_OR_TCP, l.right);
                    break;
                case 8:
                    default_config.SetAttribute(VERBOSE, l.right);
                    break;

                default:
                    break;
            }
        } else {
            Parser user = default_config;
            user.SetUser(l.left, l.right);
            users_.push_back(user);
        }
    }

    //Start Running processes for users

    for (Parser &p : users_) {
        pids.push_back({p.GetAttribute(REMOTE_PORT), RunUser(p)});
    }

    //Write the pid list to a file

    ofstream writer;
    writer.open(config_ + ".pidmap");
    for (PIDInfo &p : pids) {
        writer << p.port + ": " + p.pid + "\n";
    }
    writer.close();


    //Clean up
    util::RemoveFile("SS.conf");
    for (Parser &p : users_) {
        util::RemoveFile(p.GetAttribute(REMOTE_PORT) + ".pid");
    }
}

string ymt::RunUser(Parser &p) {

    string pid_buffer;

    vector <string> config = p.GetConfig();
    string file_buffer;

    for (const string &line : config) {
        file_buffer += line + "\n";
    }

    //Write user config
    util::WriteFile("SS.conf", {file_buffer});

    system(string("ss-server -c SS.conf " + string(p.GetAttribute(VERBOSE) == "true" ? "-v" : "") + " " + extra_param_ +
                  " -f " + p.GetAttribute(REMOTE_PORT) + ".pid").c_str());

    //Return pid buffer
    return util::ReadFile(p.GetAttribute(REMOTE_PORT) + ".pid", false)[0];
}

void ymt::StopConfig() {
    vector <string> config;
    YAML yaml;
    int fails = 0;
    int goods = 0;

    //Clean syslog first
    CleanSyslog();

    config = util::ReadFile(config_ + ".pidmap");


    if (config.empty()) {
        util::ReportError(
                "Cannot read the pidmap for the file specified, please load the config before unload and DO NOT delete pidmap.");
        return;
    }

    for (auto &line : config) {
        yaml = util::GetYaml(line);

        if (goods > 3 || util::IsProcessAlive(yaml.right)) {
            util::SysExecute("kill -15 " + yaml.right);
            goods += 1;
        } else {
            fails++;
            if (fails >= 5) break;
        };
    }

    util::RemoveFile(config_ + string(".pidmap"));
}

void
ymt::CheckPort(string port) {

    vector <string> pidmap;
    YAML yaml_line;
    string target_pid = "-1";

    pidmap = util::ReadFile(config_ + ".pidmap");

    //Quit if cannot find the pidmap file
    if (pidmap.empty()) {
        util::ReportError(
                "Cannot read the pidmap for the file specified, please load the config before unload and DO NOT delete pidmap.");
        return;
    }

    //Assume the "port" parameter is a port
    for (string &line : pidmap) {

        yaml_line = util::GetYaml(line);

        if (yaml_line.left == port) {
            target_pid = yaml_line.right;
            break;
        }
    }

    if (target_pid == "-1") {

        //Now assume the "port parameter" is actually a PID
        for (string &line : pidmap) {

            yaml_line = util::GetYaml(line);

            if (yaml_line.right == port) {
                target_pid = yaml_line.right;
                port = yaml_line.left;
                break;
            }

        }

        //Still no found...
        if (target_pid == "-1") { return; }
    }

    vector <string> file_buffer = GetLog(target_pid);

    //Some basic info
    printf("ss-manager information\n"
           "=========================\n"
           "Configuration: %s\n"
           "Port:          %s\n"
           "PID:           %s\n\n"
           "Log from system about this user:\n\n",
           config_.c_str(),
           port.c_str(),
           target_pid.c_str());

    //Print actual log
    for (string &line : file_buffer) {
        printf("%s\n", line.c_str());
    }
    printf("\n"
           "=========================\n");
}

void
ymt::SetExtraParam(string extra_param) {
    extra_param_ = extra_param;
}

void ymt::SetAttribute(YMTAttributes attribute, string value) {
    switch (attribute) {
        case CONFIG_FILENAME:
            config_ = value;
            break;
        case LOG_INPUT_FILENAME:
            input_log_ = value;
            break;
        case EXTRA_PARAM:
            extra_param_ = value;
            break;

    }
}

vector <string> ymt::GetLog(string pid) {

    vector <string> r;

    if (log_buffer_.empty()) {
        UpdateLog();
    }

    for (string &line : log_buffer_) {
        if (line.find(R"(ss-server[)" + pid + ']') != -1) {
            r.push_back(line);
        }
    }

    return r;
}

vector <PIDInfo> ymt::GetPIDTable() {

    vector <PIDInfo> r;

    vector <string> file_buffer = util::ReadFile(config_ + ".pidmap");
    YAML yaml_buffer;

    for (string &line : file_buffer) {
        yaml_buffer = util::GetYaml(line);

        r.push_back({yaml_buffer.left, yaml_buffer.right});
    }

    return r;
}

void ymt::UpdatePIDTable() {
    pid_table_ = GetPIDTable();
}

vector <SSLog> ymt::GetFormattedData() {

    vector <SSLog> log_buffer;
    string last_pid, last_port;
    bool is_pid_matched = false;

    //Get PID table (if and only if it is the first time across the program)
    if (pid_table_.empty()) { UpdatePIDTable(); }

    //Get Log (if and only if it is the first time across the program)
    if (log_buffer_.empty()) { UpdateLog(); }

    printf("Formatting Data...\n");

    for (int i = 0; i < log_buffer_.size(); ++i) {

        if (i + 1 == log_buffer_.size() || i % 100 == 0) {
            util::PercentageBar(i + 1, log_buffer_.size());
        }

        string temp_pid = util::SubString(log_buffer_[i], log_buffer_[i].find("ss-server[") + 10,
                                          log_buffer_[i].find(']'));

        if (last_pid.empty() || temp_pid != last_pid) {
            for (PIDInfo &n : pid_table_) {
                if (n.pid == temp_pid) {

                    is_pid_matched = true;

                    last_port = n.port;
                    last_pid = n.pid;
                    break;
                }
            }
        } else { is_pid_matched = true; }

        if (!is_pid_matched) continue;

        int context1_location = log_buffer_[i].find("connect to ");


        vector<size_t> s_temp = util::SearchString(log_buffer_[i],' ', 0, log_buffer_[i].find("ss-server"));
        int context2_location = s_temp[s_temp.size() - 2];

        if (context1_location != -1) {
            int addr_location = context1_location + 11;

            string time = util::SubString(log_buffer_[i], 0, context2_location);
            string destination = util::SubString(log_buffer_[i], addr_location, log_buffer_.size());

            //Push to log buffer
            log_buffer.push_back({time, last_port, last_pid, destination, CONNECT});
        }
        is_pid_matched = false;
    }

    return log_buffer;
}

vector <string> ymt::GetFormattedStringData() {
    vector <string> r;

    r.push_back("Time\tPort\tPID\tDestination");

    for (SSLog &i : GetFormattedData()) {
        r.push_back(i.time + "\t" + i.port + "\t" + i.pid + "\t" + i.destination);
    }
    return r;
}

vector <string> ymt::GetStatisics() {

    vector <SSLog> log = GetFormattedData();
    vector <InquiryData> site_list;
    vector <InquiryData> port_list;
    vector<long long> temp;
    vector <string> r;
    vector <size_t> data_seq;

    size_t LOG_SIZE = log.size();

    //Get website frequency data

    printf("Analyzing website data...\n");

    for (int i = 0; i < LOG_SIZE; ++i) {

        if (i + 1 == LOG_SIZE || i % 100 == 0) {
            util::PercentageBar(i + 1, LOG_SIZE);
        }

        bool is_website_matched = false;
        for (auto &n : site_list) {
            if (log[i].destination == n.key) {
                n.value += 1;
                is_website_matched = true;
            }
        }
        if (is_website_matched) continue;
        else {
            site_list.push_back({log[i].destination, 1});
        }
    }

    for (auto &i : site_list) {
        temp.push_back((long long) i.value);
    }

    data_seq = util::QuickSort::Sort(temp);

    //Push web frequency data
    r.push_back("Website\tInquiry Count");

    for (int i = data_seq.size() - 1; i >= 0; i--) {
        r.push_back(site_list[data_seq[i]].key + string("\t") + to_string((int) (site_list[data_seq[i]].value)));
    }

    r.push_back("");

    //Get port frequency data
    printf("Analyzing Port data...\n");

    for (int i = 0; i < LOG_SIZE; ++i) {

        if (i + 1 == LOG_SIZE || i % 100 == 0) {
            util::PercentageBar(i + 1, LOG_SIZE);
        }

        bool is_port_matched = false;
        for (auto &n : port_list) {
            if (log[i].port == n.key) {
                n.value += 1;
                is_port_matched = true;
            }
        }
        if (is_port_matched) continue;
        else {
            port_list.push_back({log[i].port, 1});
        }
    }

    //Sort Port Frequency Data

    temp.clear();
    data_seq.clear();

    for (InquiryData &i : port_list) {
        temp.push_back(i.value);
    }

    data_seq = util::QuickSort::Sort(temp);

    //Push port frequency data
    r.push_back("User Port\tInquiry Count");
    r.push_back("");

    for (int i = data_seq.size() - 1; i >= 0; --i) {
        r.push_back(port_list[data_seq[i]].key + string("\t") + to_string((int) (port_list[data_seq[i]].value)));
    }

    return r;
}

void ymt::SetFileName(string filename) {
    config_ = filename;
}

void ymt::UpdateLog() {

    vector <string> temp = util::ReadFile((input_log_.empty() ? "/var/log/syslog" : input_log_));

    for (string &i : temp) {
        if (i.find("ss-server") != -1) {
            log_buffer_.push_back(i);
        }
    }

}

void ymt::CleanSyslog() {
    util::SysExecute("rm /var/log/syslog");
    util::SysExecute("touch /var/log/syslog");
    util::SysExecute("chmod 755 /var/log/syslog");
    util::SysExecute("service rsyslog restart");
}
