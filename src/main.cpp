/**
* Slamtec Robot Go Action and Get Path Demo
*
* Created By Jacky Li @ 2014-8-8
* Copyright (c) 2014 Shanghai SlamTec Co., Ltd.
*/
#include <regex>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <rpos/robot_platforms/slamware_core_platform.h>
#include <rpos/features/location_provider/map.h>
#include <rpos/robot_platforms/objects/composite_map_reader.h>

#include <cstdarg>
#include <cstdlib>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace rpos::robot_platforms;
using namespace rpos::features;
using namespace rpos::features::location_provider;

static const int battery_low_level = 20;

std::string ipaddress = "192.168.11.1";
const std::string mapPath = "/home/nvidia/Downloads/fm0731.stcm";

std::string audio_path("/opt/apollo/uploads/");

static void write_log(const char *format, ...)
{
    va_list va;

    static FILE *fp = NULL;
    if (!fp) {
        fp = fopen("/tmp/apollo.log", "w");
    }
    if (fp) {
        va_start(va, format);
        vfprintf(fp, format, va);
        va_end(va);
        fflush(fp);
    }
}

static void write_pid_file(void)
{
    static const char fpath[] = "/var/run/apollod.pid";

    int fd = open(fpath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        printf("pid file open failed\n");
        exit(1);
    }

    if (lockf(fd, F_TLOCK, 0) < 0) {
        printf("pid file lock failed\n");
        exit(0);
    }

    char str[32];
    int n = snprintf(str, sizeof(str), "%d\n", getpid());
    if (write(fd, str, n) != n) {
        printf("pid file write failed\n");
        exit(0);
    }
}

static int do_getstatus(SlamwareCorePlatform &sdp)
{
    rpos::actions::MoveAction action = sdp.getCurrentAction();

    rpos::core::ActionStatus status = action.getStatus();
    std::cout << "status " << status << std::endl;

    if (status == rpos::core::ActionStatusError) {
        std::cout << "Reason: " << action.getReason() << std::endl;
    }

    return 0;
}

static int do_loadmap(SlamwareCorePlatform &sdp, const char *path, double x, double y)
{
    printf("load map %s\n", path);

    write_log("loading map ...");

    try {
        objects::CompositeMapReader cmapReader;
        auto cmap = cmapReader.loadFile(path);

        rpos::core::Pose pose = rpos::core::Pose(rpos::core::Location(x, y, 0));
        sdp.setCompositeMap(*cmap, pose);

        rpos::core::Pose home_pose = rpos::core::Pose(rpos::core::Location(x, y, 0));
        sdp.setHomePose(home_pose);

        std::cout << "reload map success" << std::endl;
        write_log("success");
    }
    catch (rpos::system::detail::ExceptionBase &e) {
        std::cout << "failed on " << e.what() << std::endl;
        write_log("failed");
        return -1;
    }

    return 0;
}

static int do_cancel(SlamwareCorePlatform &sdp)
{
    rpos::actions::MoveAction action = sdp.getCurrentAction();
    action.cancel();
    return 0;
}

static int do_gohome(SlamwareCorePlatform &sdp)
{
    int retry = 5;

    write_log("go home ...");
    while (retry--) {
        std::cout << "go home ..." << std::endl;

        rpos::actions::MoveAction action = sdp.getCurrentAction();

        action = sdp.goHome();

        std::cout << "wait until done" << std::endl;
        rpos::core::ActionStatus status = action.waitUntilDone();
        std::cout << "... done, status " << status << std::endl;

        if (status == rpos::core::ActionStatusFinished) {
            write_log("success");
            return 0;
        }

        std::cout << "Reason: " << action.getReason() << std::endl;

        boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
    }

    write_log("failed");
    return -1;
}

static int do_getpose(SlamwareCorePlatform &sdp)
{
    rpos::core::Pose pose = sdp.getPose();
    std::cout << "Robot Pose: " << std::endl;
    std::cout << "x: " << pose.x() << ", ";
    std::cout << "y: " << pose.y() << ", ";
    std::cout << "yaw: " << pose.yaw() << std::endl;

    std::cout << "map update " << (sdp.getMapUpdate() ? "true" : "false") << std::endl;

    return 0;
}

static int do_getbattery(SlamwareCorePlatform &sdp)
{
    int battPercentage = sdp.getBatteryPercentage();
    std::cout <<"Battery: " << battPercentage << "%" << std::endl;
    return 0;
}

static bool endsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.rfind(suffix) == (s.size()-suffix.size());
}

static bool parse_task(const char *fpath, std::vector<rpos::core::Location> &points)
{
    try {
        namespace pt = boost::property_tree;
        pt::ptree iroot;
        pt::read_json(fpath, iroot);

        std::string name = iroot.get<std::string>("name");
        std::cout << "execute task: " << name << std::endl;

        std::string audio = iroot.get<std::string>("audio1");
        std::cout << "audio file: " << audio << std::endl;

        audio_path += audio;

        std::cout << "audio file: " << audio_path << std::endl;

        for (pt::ptree::value_type &row : iroot.get_child("milestones")) {
            float x = row.second.get<float>("x");
            float y = row.second.get<float>("y");
            std::cout << x << ", " << y << std::endl;
            points.push_back(rpos::core::Location(x, y));
        }
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }

    if (points.size() == 0) {
        std::cout << "no milestones" << std::endl;
        return false;
    }

    return true;
}

static bool move_to_point(SlamwareCorePlatform &sdp, const rpos::core::Location &point)
{
    rpos::core::ActionStatus status;
    int retry = 3;

    while (retry--) {
        std::cout << "\n\n--------------------" << std::endl;
        std::cout << "moveto (" << point.x() << ", " << point.y() << ")" << std::endl;

        rpos::actions::MoveAction action = sdp.moveTo(point, false, true);

        std::cout << "wait until done" << std::endl;
        status = action.waitUntilDone();
        std::cout << "... done, status " << status << std::endl;

        if (status == rpos::core::ActionStatusFinished) {
            std::cout << "move successed" << std::endl;
            return true;
        }

        if (status == rpos::core::ActionStatusError) {
            std::string reason = action.getReason();
            std::cout << "Reason: " << reason << std::endl;

            if (action.getReason() == "aborted") {
                std::cout << "move aborted" << std::endl;
                write_log("moveto (%f, %f)\n", point.x(), point.y());
                write_log("task aborted");
                return false;
            }
            else if (action.getReason() == "unreachable") {
                std::cout << "move unreachable" << std::endl;
                write_log("moveto (%f, %f)\n", point.x(), point.y());
                write_log("task unreachable");
                return false;
            }
            else if (action.getReason() == "failed") {
                std::cout << "move failed" << std::endl;
            }
        }

        boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
    }

    return true;
}

static bool move_by_points(SlamwareCorePlatform &sdp, std::vector<rpos::core::Location> &points)
{
    for (std::vector<rpos::core::Location>::const_iterator it = points.begin(); it != points.end(); it++) {

        if (!move_to_point(sdp, *it)) {
            return false;
        }

        int battPercentage = sdp.getBatteryPercentage();
        if (battPercentage < battery_low_level) {
            return false;
        }
    }

    return true;
}

static int do_task(SlamwareCorePlatform &sdp, const char *jsonfile)
{
    rpos::actions::MoveAction action = sdp.getCurrentAction();
    rpos::core::ActionStatus status = action.getStatus();

    if (status == rpos::core::ActionStatusRunning) {
        std::cout << "apollo is running, exit now" << std::endl;
        return -1;
    }

    std::vector<rpos::core::Location> points;

    if (parse_task(jsonfile, points) == false) {
        return -1;
    }

    std::cout << "milestones " << points.size() << std::endl;


    write_log("do task ...");

    // start play music
    system("killall mpg123");

    if (endsWith(audio_path, ".mp3") && !access(audio_path.c_str(), F_OK)) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mpg123 --loop \"-1\" -a hw:2,0 %s &", audio_path.c_str());
        printf("%s\n", cmd);
        system(cmd);
    } else {
        std::cout << audio_path << " not exist, no mp3" << std::endl;
    }

    for (;;) {

        if (!move_by_points(sdp, points)) {
            break;
        }

        std::reverse(points.begin(), points.end());
    }

    int battPercentage = sdp.getBatteryPercentage();
    if (battPercentage < battery_low_level) {
        std::cout <<"Battery: " << battPercentage << "%" << std::endl;
        do_gohome(sdp);
    }

    // stop play music
    system("killall mpg123");

    return 0;
}

int main(int argc, char *argv[])
{
    const char *optstring = "x:y:sl:t:chpb";

    struct option opts[] = {
        { "status",  0, NULL, 's' },
        { "loadmap", 1, NULL, 'l' },
        { "task",    1, NULL, 't' },
        { "cancel",  0, NULL, 'c' },
        { "gohome",  0, NULL, 'h' },
        { "pose",    0, NULL, 'p' },
        { "battery", 0, NULL, 'b' },
    };

    SlamwareCorePlatform sdp;
    try {
        sdp = SlamwareCorePlatform::connect(ipaddress, 1445);
        printf("SDK Version: %s\n", sdp.getSDKVersion().c_str());
        printf("SDP Version: %s\n", sdp.getSDPVersion().c_str());
    } catch(ConnectionTimeOutException& e) {
        std::cout <<e.what() << std::endl;
        return -2;
    } catch(ConnectionFailException& e) {
        std::cout <<e.what() << std::endl;
        return -3;
    }
    //printf("Connection Successfully\n");

    if (sdp.getMapUpdate()) {
        sdp.setMapUpdate(false);
    }

    int ch;
    int ret = 0;

    double x = 0;
    double y = 0;

    const char *map = NULL;
   
    while ((ch = getopt_long(argc, argv, optstring, opts, NULL)) != -1) {
        switch (ch) {
        case 'x':
            x = strtod(optarg, NULL);
            break;
        case 'y':
            y = strtod(optarg, NULL);
            break;
        case 's':
            ret = do_getstatus(sdp);
            break;
        case 'l':
            map = strdup(optarg);
            break;
        case 't':
            write_pid_file();
            ret = do_task(sdp, optarg);
            break;
        case 'c':
            ret = do_cancel(sdp);
            break;
        case 'h':
            write_pid_file();
            ret = do_gohome(sdp);
            break;
        case 'p':
            ret = do_getpose(sdp);
            break;
        case 'b':
            ret = do_getbattery(sdp);
            break;
        default:
            break;
        }
    }

    if (map) {
        write_pid_file();
        printf("x %f, y %f, %s\n", x, y, map);
        ret =  do_loadmap(sdp, map, x, y);
    }

    sdp.disconnect();

    return ret;
}
