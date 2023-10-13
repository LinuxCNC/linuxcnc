#include "kinfamtest.hpp"
#include <frames_io.hpp>
#include <kinfam_io.hpp>
#include <chainfksolverpos_recursive.hpp>

CPPUNIT_TEST_SUITE_REGISTRATION( KinFamTest );

#ifdef  __APPLE__
typedef unsigned int uint;
#endif

using namespace KDL;
using namespace std;

void KinFamTest::setUp()
{
}

void  KinFamTest::tearDown()
{
}

void KinFamTest::JointTest()
{
    double q;
    Joint j;
    j=Joint("Joint 1", Joint::None);
    CPPUNIT_ASSERT_EQUAL(Joint::None,j.getType());
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame::Identity());
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist::Zero());
    random(q);
    j=Joint("Joint 2", Joint::RotX);
    CPPUNIT_ASSERT_EQUAL(Joint::RotX,j.getType());
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame(Rotation::RotX(q)));
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist(Vector::Zero(),Vector(q,0,0)));
    random(q);
    j=Joint("Joint 3", Joint::RotY);
    CPPUNIT_ASSERT_EQUAL(Joint::RotY,j.getType());
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame(Rotation::RotY(q)));
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist(Vector::Zero(),Vector(0,q,0)));
    random(q);
    j=Joint("Joint 4", Joint::RotZ);
    CPPUNIT_ASSERT_EQUAL(Joint::RotZ,j.getType());
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame(Rotation::RotZ(q)));
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist(Vector::Zero(),Vector(0,0,q)));
    random(q);
    j=Joint("Joint 5", Joint::TransX);
    CPPUNIT_ASSERT_EQUAL(Joint::TransX,j.getType());
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame(Vector(q,0,0)));
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist(Vector(q,0,0),Vector::Zero()));
    random(q);
    j=Joint("Joint 6", Joint::TransY);
    CPPUNIT_ASSERT_EQUAL(Joint::TransY,j.getType());
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame(Vector(0,q,0)));
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist(Vector(0,q,0),Vector::Zero()));
    random(q);
    j=Joint("Joint 7", Joint::TransZ);
    CPPUNIT_ASSERT_EQUAL(Joint::TransZ,j.getType());
    CPPUNIT_ASSERT_EQUAL(j.pose(q),Frame(Vector(0,0,q)));
    random(q);
    CPPUNIT_ASSERT_EQUAL(j.twist(q),Twist(Vector(0,0,q),Vector::Zero()));

}

void KinFamTest::SegmentTest()
{
    Segment s;
    double q,qdot;
    Frame f,f1;
    random(f);
    s = Segment("Segment 0", Joint("Joint 0", Joint::None),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
    random(f);
    s = Segment("Segment 1", Joint("Joint 1", Joint::RotX),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
    random(f);
    s = Segment("Segment 3", Joint("Joint 3", Joint::RotY),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
    random(f);
    s = Segment("Segment 4", Joint("Joint 4", Joint::RotZ),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
    random(f);
    s = Segment("Segment 5", Joint("Joint 5", Joint::TransX),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
    random(f);
    s = Segment("Segment 6", Joint("Joint 6", Joint::TransY),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
    random(f);
    s = Segment("Segment 7", Joint("Joint 7", Joint::TransZ),f);
    random(q);
    random(qdot);
    f1=s.getJoint().pose(q)*f;
    CPPUNIT_ASSERT_EQUAL(f1,s.pose(q));
    CPPUNIT_ASSERT_EQUAL(s.getJoint().twist(qdot).RefPoint(f1.p),s.twist(q,qdot));
}

void KinFamTest::ChainTest()
{
    Chain chain1;

    chain1.addSegment(Segment("Segment 0", Joint("Joint 0", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.0))));
    chain1.addSegment(Segment("Segment 1", Joint("Joint 1", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.9))));
    chain1.addSegment(Segment("Segment 2", Joint("Joint 2", Joint::RotX),
                              Frame(Vector(0.0,0.0,1.2))));
    chain1.addSegment(Segment("Segment 3", Joint("Joint 3", Joint::RotZ),
                              Frame(Vector(0.0,0.0,1.5))));
    chain1.addSegment(Segment("Segment 4", Joint("Joint 4", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.0))));
    chain1.addSegment(Segment("Segment 5", Joint("Joint 5", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.4))));
    CPPUNIT_ASSERT_EQUAL(chain1.getNrOfJoints(),(uint)6);
    CPPUNIT_ASSERT_EQUAL(chain1.getNrOfSegments(),(uint)6);
    chain1.addSegment(Segment("Segment 6", Joint("Joint 6", Joint::None),Frame(Vector(0.0,0.1,0.0))));
    CPPUNIT_ASSERT_EQUAL(chain1.getNrOfJoints(),(uint)6);
    CPPUNIT_ASSERT_EQUAL(chain1.getNrOfSegments(),(uint)7);

    Chain chain2 = chain1;
    CPPUNIT_ASSERT_EQUAL(chain2.getNrOfJoints(),chain1.getNrOfJoints());
    CPPUNIT_ASSERT_EQUAL(chain2.getNrOfSegments(),chain1.getNrOfSegments());
    chain2.addChain(chain1);
    CPPUNIT_ASSERT_EQUAL(chain2.getNrOfJoints(),chain1.getNrOfJoints()*(uint)2);
    CPPUNIT_ASSERT_EQUAL(chain2.getNrOfSegments(),chain1.getNrOfSegments()*(uint)2);
}

// forward declaration, see below
bool isSubtree(const SegmentMap::const_iterator container, const SegmentMap::const_iterator contained);

void KinFamTest::TreeTest()
{
    Tree tree1;
    Segment segment1("Segment 1", Joint("Joint 1", Joint::None));
    Segment segment2("Segment 2", Joint("Joint 2", Joint::RotX),Frame(Vector(0.1,0.2,0.3)));
    Segment segment3("Segment 3", Joint("Joint 3", Joint::TransZ),Frame(Rotation::RotX(1.57)));
    Segment segment4("Segment 4", Joint("Joint 4", Joint::RotX),Frame(Vector(0.1,0.2,0.3)));
    Segment segment5("Segment 5", Joint("Joint 5", Joint::RotX),Frame(Vector(0.1,0.2,0.3)));
    Segment segment6("Segment 6", Joint("Joint 6", Joint::RotX),Frame(Vector(0.1,0.2,0.3)));
    Segment segment7("Segment 7", Joint("Joint 7", Joint::RotX),Frame(Vector(0.1,0.2,0.3)));

    cout<<tree1<<endl;

    CPPUNIT_ASSERT(tree1.addSegment(segment1,"root"));
    CPPUNIT_ASSERT(tree1.addSegment(segment2,"root"));
    CPPUNIT_ASSERT(tree1.addSegment(segment3,"Segment 1"));
    CPPUNIT_ASSERT(tree1.addSegment(segment4,"Segment 3"));
    CPPUNIT_ASSERT(!tree1.addSegment(segment1,"Segment 6"));
    CPPUNIT_ASSERT(!tree1.addSegment(segment1,"Segment 4"));

    cout<<tree1<<endl;

    Tree tree2;
    CPPUNIT_ASSERT(tree2.addSegment(segment5,"root"));
    CPPUNIT_ASSERT(tree2.addSegment(segment6,"root"));
    CPPUNIT_ASSERT(tree2.addSegment(segment7,"Segment 6"));

    cout<<tree2<<endl;

    Chain chain1;
    chain1.addSegment(Segment("Segment 8", Joint("Joint 8", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.0))));
    chain1.addSegment(Segment("Segment 9", Joint("Joint 9", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.9))));
    chain1.addSegment(Segment("Segment 10", Joint("Joint 10", Joint::RotX),
                              Frame(Vector(0.0,0.0,1.2))));
    chain1.addSegment(Segment("Segment 11", Joint("Joint 11", Joint::RotZ),
                              Frame(Vector(0.0,0.0,1.5))));
    chain1.addSegment(Segment("Segment 12", Joint("Joint 12", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.0))));
    chain1.addSegment(Segment("Segment 13", Joint("Joint 13", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.4))));


    CPPUNIT_ASSERT(tree2.addChain(chain1, "Segment 6"));
    cout<<tree2<<endl;
    CPPUNIT_ASSERT(tree1.addTree(tree2, "Segment 2"));
    cout<<tree1<<endl;

    Chain extract_chain1;
    CPPUNIT_ASSERT(tree1.getChain("Segment 2", "Segment 4", extract_chain1));
    Chain extract_chain2;
    CPPUNIT_ASSERT(tree1.getChain("Segment 4", "Segment 2", extract_chain2));
    CPPUNIT_ASSERT(tree1.getChain("Segment 4", "Segment 2", extract_chain2));
    CPPUNIT_ASSERT(extract_chain1.getNrOfJoints()==extract_chain2.getNrOfJoints());
    CPPUNIT_ASSERT(extract_chain1.getNrOfSegments()==extract_chain2.getNrOfSegments());
    ChainFkSolverPos_recursive solver1(extract_chain1);
    ChainFkSolverPos_recursive solver2(extract_chain2);


    Frame f1, f2;
    JntArray jnt1(extract_chain2.getNrOfJoints());
    JntArray jnt2(extract_chain2.getNrOfJoints());
    for (int i=0; i<(int)extract_chain2.getNrOfJoints(); i++){
      jnt1(i) = (i+1)*2;
      jnt2((int)extract_chain2.getNrOfJoints()-i-1) = jnt1(i);
    }
    solver1.JntToCart(jnt1, f1);
    solver2.JntToCart(jnt2, f2);
    CPPUNIT_ASSERT(f1 == f2.Inverse());

    Tree subtree;
    const std::string subroot("Segment 2");
    CPPUNIT_ASSERT(tree1.getSubTree(subroot, subtree));
    cout << "Tree 1:" << endl << tree2str(tree1) << endl;
    cout << "Subtree (rooted at " << subroot << "):" << endl << tree2str(subtree) << endl;
    CPPUNIT_ASSERT(isSubtree(tree1.getSegment(subroot), subtree.getRootSegment()));
    CPPUNIT_ASSERT(isSubtree(subtree.getRootSegment(), tree1.getSegment(subroot)));

    Segment segment101("Segment 101", Joint("Joint 101", Joint::RotZ), Frame(Vector(0.0,0.0,0.5)));
    Segment segment102("Segment 102", Joint("Joint 102", Joint::RotZ), Frame(Vector(0.0,0.0,1.0)));
    subtree.addSegment(segment101, subtree.getRootSegment()->first);
    subtree.addSegment(segment102, subtree.getSegment("Segment 5")->first);
    cout << "Subtree (rooted at " << subroot << "):" << endl << tree2str(subtree) << endl;
    CPPUNIT_ASSERT(!isSubtree(tree1.getSegment(subroot), subtree.getRootSegment()));
    CPPUNIT_ASSERT(isSubtree(subtree.getRootSegment(), tree1.getSegment(subroot)));
}

//Utility to check if the set of segments in contained is a subset of container.
//In addition, all the children of a segment in contained must be present in
//container as children of the same segment.
bool isSubtree(const SegmentMap::const_iterator container, const SegmentMap::const_iterator contained) {
    //Check that the container and contained point to the same link
    if(container->first != contained->first)
        return false;
    //Check that each child of contained is a child of container
    std::vector<SegmentMap::const_iterator> children = GetTreeElementChildren(contained->second);
    for(unsigned int i=0; i < children.size(); i++) {
        //look for a child of container whose name matches the one of the current child from contained
        std::vector<SegmentMap::const_iterator>::const_iterator it = GetTreeElementChildren(container->second).begin();
        while(it != GetTreeElementChildren(container->second).end()) {
            if((*it)->first == children[i]->first)
                break; //segment found, exit the loop
            it++;
        }
        if(it == GetTreeElementChildren(container->second).end())
            return false; //child of contained not found as child of container
        //inspect recursively all the children
        if(!isSubtree((*it), children[i]))
            return false;
    }
    return true;
}
