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
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace rpos::robot_platforms;
using namespace rpos::features;
using namespace rpos::features::location_provider;

std::string ipaddress = "192.168.11.1";
const std::string mapPath = "/home/nvidia/Downloads/fm0731.stcm";

static void slog(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    vsyslog(LOG_USER|LOG_INFO, format, va);
    va_end(va);
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

    try {
        objects::CompositeMapReader cmapReader;
        auto cmap = cmapReader.loadFile(path);

        rpos::core::Pose pose = rpos::core::Pose(rpos::core::Location(x, y, 0));
        sdp.setCompositeMap(*cmap, pose);

        rpos::core::Pose home_pose = rpos::core::Pose(rpos::core::Location(x, y, 0));
        sdp.setHomePose(home_pose);

        std::cout << "reload map success" << std::endl;
    }
    catch (rpos::system::detail::ExceptionBase &e) {
        std::cout << "failed on " << e.what() << std::endl;
        return -1;
    }

    return 0;
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

    try {
        namespace pt = boost::property_tree;
        pt::ptree iroot;
        pt::read_json(jsonfile, iroot);
        
        std::string name = iroot.get<std::string>("name");
        std::cout << "execute task: " << name << std::endl;

        for (pt::ptree::value_type &row : iroot.get_child("milestones")) {
            float x = row.second.get<float>("x");
            float y = row.second.get<float>("y");
            std::cout << x << ", " << y << std::endl;
            points.push_back(rpos::core::Location(x, y));
        }
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    if (points.size() == 0) {
        std::cout << "no milestones" << std::endl;
        return 1;
    }

    std::cout << "milestones " << points.size() << std::endl;

    for (std::vector<rpos::core::Location>::const_iterator it = points.begin(); it != points.end(); it++) {

        rpos::core::ActionStatus status;
        int retry = 3;

        while (retry) {
            std::cout << "\n\n--------------------" << std::endl;
            std::cout << "moveto (" << it->x() << ", " << it->y() << ")" << std::endl;

            rpos::actions::MoveAction action = sdp.moveTo(*it, false, true);

            std::cout << "wait until done" << std::endl;
            status = action.waitUntilDone();
            std::cout << "... done, status " << status << std::endl;

            if (status == rpos::core::ActionStatusFinished) {
                std::cout << "move successed" << std::endl;
                break;
            }

            if (status == rpos::core::ActionStatusError) {
                std::string reason = action.getReason();
                std::cout << "Reason: " << reason << std::endl;

                if (action.getReason() == "aborted") {
                    std::cout << "move aborted" << std::endl;
                    break;
                }

                std::cout << "move failed" << std::endl;
            }

            boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
            retry--;
        }
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
    int retry = 3;

    while (retry--) {
        std::cout << "go home ..." << std::endl;

        rpos::actions::MoveAction action = sdp.getCurrentAction();

        action = sdp.goHome();

        std::cout << "wait until done" << std::endl;
        rpos::core::ActionStatus status = action.waitUntilDone();
        std::cout << "... done, status " << status << std::endl;

        if (status == rpos::core::ActionStatusFinished) {
            return 0;
        }

        std::cout << "Reason: " << action.getReason() << std::endl;

        boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
    }

    return -1;
}

static int do_getpose(SlamwareCorePlatform &sdp)
{
    rpos::core::Pose pose = sdp.getPose();
    //slog("Robot Pose: (%f, %f) yaw %f\n", pose.x(), pose.y(), pose.yaw());
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

int main(int argc, char *argv[])
{
    slog(argv[0]);

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
        slog("SDK Version: %s\n", sdp.getSDKVersion().c_str());
        slog("SDP Version: %s\n", sdp.getSDPVersion().c_str());
    } catch(ConnectionTimeOutException& e) {
        std::cout <<e.what() << std::endl;
        return -2;
    } catch(ConnectionFailException& e) {
        std::cout <<e.what() << std::endl;
        return -3;
    }
    printf("Connection Successfully\n");

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
            ret = do_task(sdp, optarg);
            break;
        case 'c':
            ret = do_cancel(sdp);
            break;
        case 'h':
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
        printf("x %f, y %f, %s\n", x, y, map);
        ret =  do_loadmap(sdp, map, x, y);
    }

    sdp.disconnect();

    return ret;
}
