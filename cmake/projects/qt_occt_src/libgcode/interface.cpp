#include "interface.h"

interface::interface()
{

}

std::vector<interface::block> interface::read_gcode_file(std::string filename){

    block blk;
    std::vector<block> blkvec;

enum g_type {
    G0=0,
    G1=1,
    G2=2,
    G3=3
};

using namespace gpr;

std::ifstream t(filename.c_str());
std::string file_contents((std::istreambuf_iterator<char>(t)),
                          std::istreambuf_iterator<char>());

gcode_program p = parse_gcode(file_contents);

// Value's are only changed when value changes.
double X=0,Y=0,Z=0,I=0,J=0,K=0,F=0;

for(int i=0; i<p.num_blocks(); i++){
    //std::cout<<"gcode line chunck size:"<<p.get_block(i).size()<<std::endl; // Text editor line +1.

    for(int chunk=0; chunk<p.get_block(i).size(); chunk++){

        /*
        std::cout<<"chunk data: "<<p.get_block(i).get_chunk(chunk)<<std::endl; // Text editor line +1.
        if(p.get_block(i).get_chunk(chunk).tp()==CHUNK_TYPE_WORD_ADDRESS){ // tp=type
            std::cout<<"chunk word id: "<<p.get_block(i).get_chunk(chunk).get_word()<<std::endl; // the chunk Id.

            block b=p.get_block(i);
            if(b.get_chunk(chunk).get_address().tp()==ADDRESS_TYPE_DOUBLE){
                std::cout<<"chunk double value: "<<b.get_chunk(chunk).get_address().double_value()<<std::endl;
            }

            if(b.get_chunk(chunk).get_address().tp()==ADDRESS_TYPE_INTEGER){
                std::cout<<"chunk integer value: "<<b.get_chunk(chunk).get_address().int_value()<<std::endl;
            }

            if(b.get_chunk(chunk).tp()==CHUNK_TYPE_COMMENT){
                std::cout<<"chunk comment: "<<b.get_chunk(chunk).get_comment_text()<<std::endl; // the chunk Id.
            }
        }
        */

        // Find the character : g,G
        char a='0';
        if(p.get_block(i).get_chunk(chunk).tp()==CHUNK_TYPE_WORD_ADDRESS){ // tp=type
            a=p.get_block(i).get_chunk(chunk).get_word();
        }
        char axisletter;
        int gtype=11111;


        if(a=='G' || a=='g'){
            // std::cout<<"G found"<<std::endl;
            // Find 0,1,2,3
            gtype=p.get_block(i).get_chunk(chunk).get_address().int_value();
            // std::cout<<"G type:"<<gtype<<std::endl;

            if(gtype==0){
                // std::cout<<"G0, draw a rapid"<<std::endl;

                for(int j=chunk+1; j<p.get_block(i).size(); j++){
                    // Get the xyz values.
                    axisletter=p.get_block(i).get_chunk(j).get_word();
                    if(axisletter=='X' || axisletter=='x'){
                        X=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Y' || axisletter=='y'){
                        Y=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Z' || axisletter=='z'){
                        Z=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                }
                blk.type="G0";
                blk.X=X;
                blk.Y=Y;
                blk.Z=Z;
                blk.I=I;
                blk.J=J;
                blk.K=K;
                blk.F=F;
                blkvec.push_back(blk);
                std::cout<<"g0 x:"<<X<<" y:"<<Y<<" z:"<<Z<<" f:"<<F<<std::endl;
            }
            if(gtype==1){
                // std::cout<<"G1, draw a line"<<std::endl;

                for(int j=chunk+1; j<p.get_block(i).size(); j++){
                    // Get the xyz values.
                    axisletter=p.get_block(i).get_chunk(j).get_word();
                    if(axisletter=='X' || axisletter=='x'){
                        X=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Y' || axisletter=='y'){
                        Y=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Z' || axisletter=='z'){
                        Z=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='F' || axisletter=='f'){
                        F=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                }
                blk.type="G1";
                blk.X=X;
                blk.Y=Y;
                blk.Z=Z;
                blk.I=I;
                blk.J=J;
                blk.K=K;
                blk.F=F;
                blkvec.push_back(blk);
                std::cout<<"g1 x:"<<X<<" y:"<<Y<<" z:"<<Z<<" f:"<<F<<std::endl;
            }
            if(gtype==2){
                // std::cout<<"G2, draw a cw arc"<<std::endl;

                for(int j=chunk+1; j<p.get_block(i).size(); j++){
                    // Get the xyz values.
                    axisletter=p.get_block(i).get_chunk(j).get_word();
                    if(axisletter=='X' || axisletter=='x'){
                        X=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Y' || axisletter=='y'){
                        Y=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Z' || axisletter=='z'){
                        Z=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='I' || axisletter=='i'){
                        I=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='J' || axisletter=='j'){
                        J=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='K' || axisletter=='k'){
                        K=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='F' || axisletter=='f'){
                        F=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                }
                blk.type="G2";
                blk.X=X;
                blk.Y=Y;
                blk.Z=Z;
                blk.I=I;
                blk.J=J;
                blk.K=K;
                blk.F=F;
                blkvec.push_back(blk);
                std::cout<<"g2 x:"<<X<<" y:"<<Y<<" z:"<<Z<<" i:"<<I<<" j:"<<J<<" k:"<<K<<" f:"<<F<<std::endl;
            }
            if(gtype==3){
                // std::cout<<"G3, draw a ccw arc"<<std::endl;

                for(int j=chunk+1; j<p.get_block(i).size(); j++){
                    // Get the xyz values.
                    axisletter=p.get_block(i).get_chunk(j).get_word();
                    if(axisletter=='X' || axisletter=='x'){
                        X=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Y' || axisletter=='y'){
                        Y=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='Z' || axisletter=='z'){
                        Z=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='I' || axisletter=='i'){
                        I=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='J' || axisletter=='j'){
                        J=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='K' || axisletter=='k'){
                        K=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                    if(axisletter=='F' || axisletter=='f'){
                        F=p.get_block(i).get_chunk(j).get_address().double_value();
                    }
                }
                blk.type="G3";
                blk.X=X;
                blk.Y=Y;
                blk.Z=Z;
                blk.I=I;
                blk.J=J;
                blk.K=K;
                blk.F=F;
                blkvec.push_back(blk);
                std::cout<<"g3 x:"<<X<<" y:"<<Y<<" z:"<<Z<<" i:"<<I<<" j:"<<J<<" k:"<<K<<" f:"<<F<<std::endl;
            }
        }
    }
}

std::cout<<" "<<std::endl; // the chunk Id.
return blkvec;
}

/* .ngc example
G21 (unit mm)
G40 (cutter compensation off)
G80 (cancel canned cycle modal motion)
G90 (absolute distance, no offsets)
G64P0.01 (path following accuracy)
S2000.000000
G0 X500.000 Y-250.000 Z550.000
G0 X500.000 Y-250.000 Z500.000
M3
G1 X500.000 Y250.000 Z500.000 F2000
G1 X750.000 Y250.000 Z500.000 F2000
G1 X750.000 Y-250.000 Z500.000 F2000
G1 X500.000 Y-250.000 Z500.000 F2000
M5
G0 X500.000 Y-250.000 Z550.000 F2000
G0 X500.000 Y0.000 Z550.000 F2000
G0 X500.000 Y0.000 Z500.000 F2000
M3
G2 X750.000 Y0.000 Z500.000 I125 J0 K0.000 F2000
G2 X500.000 Y0.000 Z500.000 I-125 J0 K0.000 F2000
G0 X500.000 Y0.000 Z550.000
M30
*/
