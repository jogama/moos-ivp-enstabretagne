#include "maplocalizer.h"
#include "ibex_QInter.h"

#include <fstream>
using std::ifstream;
#include <strstream>
#include "vibes.h"


MapLocalizer::MapLocalizer(const string &map_filename):
    X_cur(2), spd_err(0.01), hdg_err(0.01)
{
    loadMap(map_filename);
}


MapLocalizer::~MapLocalizer()
{
    if(f != NULL) delete f;

}

void MapLocalizer::setInitialPosition(ibex::Interval &x, ibex::Interval &y, double &time)
{
    x_inertial = x;
    y_inertial = y;
    X_cur[0] = x;
    X_cur[1] = y;
    t_old = time;
}

void MapLocalizer::setInitialPosition(double x, double y, double time)
{
    x_inertial = Interval(x).inflate(0.01);
    y_inertial = Interval(y).inflate(0.01);
    X_cur[0] = x_inertial;
    X_cur[1] = y_inertial;
    t_old = time;
}

void MapLocalizer::update(ibex::Interval &rho, ibex::Interval &theta, double &time, int q)
{
    // Add the new measurment to the list and remove the last entry
    data.push_front(Data_t(x_inertial, y_inertial, rho, theta, time));
    pos.push_front(X_cur);

    vector<IntervalVector> boxes(data.size(), pos[0]);
    ibex::Array<IntervalVector> array_boxes(pos.size());
    // case i == 0, only contraction
    Data_t &t0 = data[0];
    IntervalVector dX(2);
    //vibes::drawBox( boxes[0][0].lb(), boxes[0][0].ub(),  boxes[0][1].lb(), boxes[0][1].ub(), "k" );
    contract(boxes[0], data[0].rho,data[0].theta);
    //vibes::drawBox( boxes[0][0].lb(), boxes[0][0].ub(),  boxes[0][1].lb(), boxes[0][1].ub(), "k[g]" );
    pos[0] &= boxes[0];
    array_boxes.set_ref(0, boxes[0]);


    for(int i = 1; i < data.size(); i++){
        Data_t &ti = data[i];
        double dx1 = t0.x.lb() - ti.x.lb();
        double dx2 = t0.x.ub() - ti.x.ub();
        dX[0] = Interval( std::min(dx1, dx2), std::max(dx1, dx2));
        double dy1 = t0.y.lb() - ti.y.lb();
        double dy2 = t0.y.ub() - ti.y.ub();
        dX[1] = Interval( std::min(dy1, dy2), std::max(dy1, dy2));
        IntervalVector old_pos = pos[i];
        const IntervalVector &cur_pos = pos[0];
        //vibes::drawBox(cur_pos[0].lb(),cur_pos[0].ub(), cur_pos[1].lb(),cur_pos[1].ub(), "k" );
        //vibes::drawBox(old_pos[0].lb(),old_pos[0].ub(), old_pos[1].lb(),old_pos[1].ub(), "[#0000FF22]" );

        IntervalVector tmp  = cur_pos  - dX;
        //vibes::drawBox(tmp[0].lb(),tmp[0].ub(), tmp[1].lb(),tmp[1].ub(), "[#FF0000AA]" );
        //vibes::drawBox(old_pos[0].lb(),old_pos[0].ub(), old_pos[1].lb(),old_pos[1].ub(), "[#FF00FF22]" );
        contract(old_pos, ti.rho,ti.theta);
        //vibes::drawBox(old_pos[0].lb(),old_pos[0].ub(), old_pos[1].lb(),old_pos[1].ub(), "[#FF000022]" );
        IntervalVector tmp1 = old_pos + dX;
        //vibes::drawBox(tmp[0].lb(),tmp[0].ub(), tmp[1].lb(),tmp[1].ub(), "[#FF000022]" );
        boxes[i] &= tmp1;
        //vibes::drawBox( boxes[i][0].lb(), boxes[i][0].ub(),  boxes[i][1].lb(), boxes[i][1].ub(), "[#00FF00]" );
        array_boxes.set_ref(i, boxes[i]);
    }


    for (int i = 0; i < boxes.size(); i++){
       // if(boxes[i].is_empty())
       //     qDebug() << "boxes " << i << "is empty !!";
       // vibes::drawBox( boxes[i][0].lb(), boxes[i][0].ub(),  boxes[i][1].lb(), boxes[i][1].ub(), "k" );
        //vibes::drawBox( pos[i][0].lb(), pos[i][0].ub(),  pos[i][1].lb(), pos[i][1].ub(), "k[g]" );
        //Data_t &t = data[i];
        //vector<double> cx = {pos[i][0].mid(), pos[i][0].mid()+t.rho.mid()*cos(t.theta.mid())};
        //vector<double> cy = {pos[i][1].mid(), pos[i][1].mid()+t.rho.mid()*sin(t.theta.mid())};
        //vibes::drawLine(cx, cy, "g");
        pos[0] &= boxes[i];
    }
    //q = std::min(1, (int)pos.size()-1);
    //pos[0] &= ibex::qinter_projf(array_boxes, pos.size() - q);
    //for
    //vibes::drawBox( pos[0][0].lb(), pos[0][0].ub(),  pos[0][1].lb(), pos[0][1].ub(), "[b]" );

    int NMax = 30;
    if(data.size() >= NMax){
        data.pop_back();
        pos.pop_back();
    }
    X_cur &= pos[0];
    //setPosition(pos[0][0], pos[0][1], time);

}


// map_filename contains list of segments
// ax1 ax1 bx1 by1
// ax2 ax2 bx2 by2
// ....
// Load the map and fill <Walls> vector

void MapLocalizer::loadMap(const string& map_filename){
    // Clear the previous map
    this->walls.clear();

    std::ifstream in_file;
    in_file.open(map_filename, ios::in);
    if(in_file.fail()) {
        std::stringstream s;
        s << "MapLocalizer [load]: cannot open file " << map_filename << "for reading the map";
        std::cerr << s << std::endl;
        exit(-1);
    }
    try {
        std::string line;
        // Read the header and fill it in with wonderful values
        while (!in_file.eof()) {

            getline (in_file, line);
            // Ignore empty lines
            if (line == "")
                continue;
            std::stringstream sstream(line);
            Wall w;
            sstream >> w[0] >> w[1] >> w[2] >> w[3];
            walls.push_back(w);
            std::cout << w[0] << " " << w[1] << " "
                              << w[2] << " " << w[3]
                              << " " << std::endl;
        }

    } catch (std::exception& e) {
        std::stringstream s;
        s << "MapLocalizer [load]: reading error " << e.what() << std::endl;
        std::cerr << s << std::endl;
    }
    in_file.close();

}

void MapLocalizer::contractSegment(Interval& x, Interval& y, Wall& wall){
    IntervalVector tmp(6);
    tmp[0] = x;
    tmp[1] = y;
    tmp[2] = Interval(wall[0]);
    tmp[3] = Interval(wall[1]);
    tmp[4] = Interval(wall[2]);
    tmp[5] = Interval(wall[3]);
    this->ctcSegment.contract(tmp);
    x &= tmp[0];
    y &= tmp[1];
    if(x.is_empty() || y.is_empty()){
        x.set_empty(); y.set_empty();
    }
}

void MapLocalizer::contractOneMeasurment(Interval&x, Interval&y, Interval& rho, Interval& theta, Wall& wall){
//    vibes::clearFigure("test");
//    vibes::drawBox(x.lb(), x.ub(), y.lb(), y.ub(), "r");
//    vector<double> wx = {wall[0], wall[2]};
//    vector<double> wy = {wall[1], wall[3]};
//    vibes::drawLine(wx, wy);


    Interval ax = rho*cos(theta);
    Interval ay = rho*sin(theta);

    Interval qx = x + ax;
    Interval qy = y + ay;

//    vibes::drawBox(qx.lb(), qx.ub(), qy.lb(), qy.ub(), "[g]");

    contractSegment(qx, qy, wall);

  //  vibes::drawBox(qx.lb(), qx.ub(), qy.lb(), qy.ub(), "[y]");

    bwd_add(qx, x, ax);
    bwd_add(qy, y, ay);

    if(x.is_empty() || y.is_empty()){
        x.set_empty(); y.set_empty();
    }
}

void MapLocalizer::contract(IntervalVector& X, Interval& rho, Interval& theta, int q){
    // List of boxes initialized with X
    std::vector<IntervalVector> boxes(walls.size(), X);
    IntervalVector res(IntervalVector::empty(2));
    for(int i = 0; i < walls.size(); i++){ 
        contractOneMeasurment(boxes[i][0], boxes[i][1], rho, theta, walls[i]);

        res |= boxes[i];
    }

    X &= res;
}


void MapLocalizer::predict(double& v, double& theta, double& t){

    Interval iV(v);
    Interval iTheta(theta);
    iV.inflate(spd_err);
    iTheta.inflate(hdg_err);

    double dt = t - t_old;

    Interval df_x = iV*cos(iTheta)*dt;
    Interval df_y = iV*sin(iTheta)*dt;
    // Update inertial tube
    x_inertial += df_x;
    y_inertial += df_y;
    // Update current position box
    X_cur[0] += df_x;
    X_cur[1] += df_y;

    t_old = t;
}



