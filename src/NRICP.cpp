#include "NRICP.h"


NRICP::NRICP(Mesh* _template,  Mesh* _target)
{
    m_template = _template;
    m_target = _target;
    m_stiffness = 200.0;
    m_epsilon = 10.0;
    m_gamma = 5.0;
    m_templateVertCount = m_template->getVertCount();
    m_targetVertCount = m_target->getVertCount();

// Defining M ============================================
    m_M = m_template->getM();
    if(!m_M)
    {
      m_template->buildArcNodeMatrix();
      m_M = m_template->getM();
    }

// Defining W =================NEW========================
    m_W = new VectorXi(m_templateVertCount);
    m_W->setOnes(m_templateVertCount);

// Defining D ========================================
    m_D = m_template->getD();
    if(!m_D)
    {
        m_template->buildVertexMatrix();
        m_D = m_template->getD();
    }

// Defining U ==================NEW========================
    m_U = new MatrixXf(m_templateVertCount, 3);
    m_U->setZero(m_templateVertCount, 3);

// Defining X and X_prev ==================NEW========================
    m_X = new MatrixXf(4 * m_templateVertCount, 3);
    m_X->setZero(4 * m_templateVertCount, 3);

// Defining Sphere Partitions ========NEW=========================================
   m_targetPartition = createPartitions(0, m_targetVertCount-1, m_targetPartition);

//Sparse matrices A and B
   m_templateEdgeCount = m_template->getEdgeCount();
   m_A = new  SparseMatrix<GLfloat> (4 * m_templateEdgeCount + m_templateVertCount, 4 * m_templateVertCount);
   m_A->reserve(8 * m_templateEdgeCount + 4 * m_templateVertCount);
   m_B = new  SparseMatrix<GLfloat> (4 * m_templateEdgeCount + m_templateVertCount, 3);
   m_B->reserve(3 * m_templateVertCount);


//Aux
   myfile.open("../logs/times6.txt");
}

NRICP::~NRICP()
{

    if(m_W)
    {
     delete m_W;
    }

    if(m_U)
    {
     delete m_U;
    }

    if(m_X)
    {
     delete m_X;
    }

    destroyPartitions(m_targetPartition);

    if(m_A)
    {
     delete m_A;
    }

    if(m_B)
    {
     delete m_B;
    }

    myfile.close();
}

SpherePartition* NRICP::createPartitions(unsigned int _start, unsigned int _end, SpherePartition* _partition)
{
    unsigned int three_i;


    //Observation: This assumes vertex data is in order
    _partition = new SpherePartition();
    _partition->start = _start;
    _partition->end = _end;
    _partition->average[0] = 0.0;
    _partition->average[1] = 0.0;
    _partition->average[2] = 0.0;

    std::vector<GLfloat>* targetVertices = m_target->getVertices();


    for(unsigned int i=_start; i<=_end; ++i)
    {
      three_i = 3*i;
      _partition->average[0] += targetVertices->at(three_i);
      _partition->average[1] += targetVertices->at(three_i + 1);
      _partition->average[2] += targetVertices->at(three_i + 2);
    }

     unsigned int noElem = _end - _start + 1;

    _partition->average[0] /= noElem;
    _partition->average[1] /= noElem;
    _partition->average[2] /= noElem;

    if(noElem > 3)
    {
     _partition->left = createPartitions(_start, _start+noElem/2, _partition->left);
     _partition->right = createPartitions(_start+noElem/2, _end, _partition->right);
    }

    return _partition;
}


void NRICP::destroyPartitions(SpherePartition* _partition)
{
    if(_partition)
    {
        if(!_partition->left && !_partition->right)
        {
            delete _partition;
        }

        if (_partition->right)
        {
           destroyPartitions(_partition->right);
        }

        if (_partition->left)
        {
           destroyPartitions(_partition->left);
        }
    }
}


void NRICP::calculateTransformation()
{
    static double previous_seconds = 0.0;
    static double current_seconds = 0.0;
    static double elapsed_seconds = 0.0;

    MatrixXf* X_prev = new MatrixXf(4 * m_templateVertCount, 3);
    X_prev->setZero(4 * m_templateVertCount, 3);

    previous_seconds = glfwGetTime();
    findCorrespondences();
    current_seconds = glfwGetTime();
    elapsed_seconds = current_seconds - previous_seconds;
    previous_seconds = current_seconds;
    myfile << " Finding correspondences takes " << elapsed_seconds << " seconds \n ";

    previous_seconds = glfwGetTime();
    determineOptimalDeformation();
    current_seconds = glfwGetTime();
    elapsed_seconds = current_seconds - previous_seconds;
    previous_seconds = current_seconds;
    myfile << " Determining the optimal deformation takes " << elapsed_seconds << " seconds \n";

    previous_seconds = glfwGetTime();
    deformTemplate();
    current_seconds = glfwGetTime();
    elapsed_seconds = current_seconds - previous_seconds;
    previous_seconds = current_seconds;
    myfile << " Deforming the template takes "<< elapsed_seconds <<" seconds \n";

    previous_seconds = glfwGetTime();
    float changes = normedDifference(X_prev, m_X);
    current_seconds = glfwGetTime();
    elapsed_seconds = current_seconds - previous_seconds;
    previous_seconds = current_seconds;
    myfile << " Calculating the normed difference takes "<< elapsed_seconds << " seconds \n";

     previous_seconds = glfwGetTime();
     while (changes > m_epsilon)
     {
      (*X_prev) = (*m_X);
      findCorrespondences();
      determineOptimalDeformation();
      deformTemplate();
      changes = normedDifference(X_prev, m_X);
     }

     current_seconds = glfwGetTime();
     elapsed_seconds = current_seconds - previous_seconds;
     previous_seconds = current_seconds;
     myfile << " While loop takes " << elapsed_seconds << " seconds \n\n\n";

     myfile.flush();

     delete X_prev;
}

float NRICP::normedDifference(MatrixXf* _Xj_1, MatrixXf* _Xj)
  {
    float norm = 0.0;
    float diff = 0.0;
    unsigned int floatCount = 4 * m_templateVertCount;

    for(unsigned int i=0; i < floatCount; ++i)
    {
        for(unsigned int j=0; j<3; ++j)
        {
            diff = (*_Xj)(i, j) - (*_Xj_1)(i, j);
            norm += diff*diff;
        }
    }
    return sqrt(norm);
  }

void NRICP::findCorrespondences()
{
      std::vector<GLfloat>* vertsTemplate = m_template->getVertices();
      Vector3f templateVertex;
      float distance_left = 0.0;
      float distance_right = 0.0;
      unsigned int three_i;

      for (unsigned int i = 0; i < m_templateVertCount; ++i)
      {
          three_i = 3*i;
          templateVertex(0) = vertsTemplate->at(three_i);
          templateVertex(1) = vertsTemplate->at(three_i + 1);
          templateVertex(2) = vertsTemplate->at(three_i + 2);
          SpherePartition* previous = NULL;
          SpherePartition* aux = NULL;
          SpherePartition* current = m_targetPartition;

          while(current)
          {
              Vector3f targetSphere_left(1000.0, 1000.0, 1000.0);
              Vector3f targetSphere_right(1000.0, 1000.0, 1000.0);

              if(current->left)
              {
                targetSphere_left[0] = current->left->average[0];
                targetSphere_left[1] = current->left->average[1];
                targetSphere_left[2] = current->left->average[2];
              }

              if(current->right)
              {
                  targetSphere_right[0] = current->right->average[0];
                  targetSphere_right[1] = current->right->average[1];
                  targetSphere_right[2] = current->right->average[2];
              }

              distance_left = euclideanDistance(templateVertex, targetSphere_left);
              distance_right = euclideanDistance(templateVertex, targetSphere_right);

              if(distance_left < distance_right)
              {
                aux = current;
                previous = current;
                current = aux->left;
              }
              else
              {
                  aux = current;
                  previous = current;
                  current = aux->right;
              }
          }

          if(previous)
          {
            findCorrespondences_Naive(i, previous->start, previous->end);
          }

          else
          {
            (*m_W)(i) = 0.0;
          }
      }

}

void NRICP::findCorrespondences_Normals(unsigned int _templateIndex, unsigned int _targetStart, unsigned int _targetEnd)
  {
    //Find closest sphere between template and target mesh
    //Find minimal angle between normals

    std::vector<GLfloat>* normalsTemplate = m_template->getNormals();
    std::vector<GLfloat>* normalsTarget = m_target->getNormals();
    std::vector<GLfloat>* vertsTarget = m_target->getVertices();

    Vector3f templateNormal;
    Vector3f targetNormal;
    float angle = 0.0;
    float maxAngle = -1.0;
    bool foundCorrespondence = false;
    Vector3f auxUi(0.0, 0.0, 0.0);

    templateNormal(0) = normalsTemplate->at(_templateIndex * 3);
    templateNormal(1) = normalsTemplate->at(_templateIndex * 3 + 1);
    templateNormal(2) = normalsTemplate->at(_templateIndex * 3 + 2);


    for(unsigned int j=_targetStart; j<=_targetEnd; ++j)
    {
     targetNormal(0) = normalsTarget->at(j * 3);
     targetNormal(1) = normalsTarget->at(j * 3 + 1);
     targetNormal(2) = normalsTarget->at(j * 3 + 2);

     angle = templateNormal.dot(targetNormal);

     if(angle >= 0 && angle <= 1.0 && angle > maxAngle)
      {
       auxUi(0) = vertsTarget->at(j * 3);
       auxUi(1) = vertsTarget->at(j * 3 + 1);
       auxUi(2) = vertsTarget->at(j * 3 + 2);
       maxAngle = angle;
       foundCorrespondence = true;
      }
     }

      if(foundCorrespondence)
      {
       (*m_U)(_templateIndex, 0) = auxUi(0);
       (*m_U)(_templateIndex, 1) = auxUi(1);
       (*m_U)(_templateIndex, 2) = auxUi(2);
      }

      else
      {
        (*m_W)(_templateIndex) = 0.0;
      }
  }

void NRICP::findCorrespondences_Naive(unsigned int _templateIndex, unsigned int _targetStart, unsigned int _targetEnd)
  {
      //Find closest points between template and target mesh
      //Store in U
      //Store values in W - see 4.4.

      std::vector<GLfloat>* vertsTemplate = m_template->getVertices();
      std::vector<GLfloat>* vertsTarget = m_target->getVertices();
      Vector3f templateVertex;
      Vector3f targetVertex;
      float distance = 0.0;
      float minDistance = 1000.0;
      bool foundCorrespondence = false;
      unsigned int targetIndex = 0;
      Vector3f auxUi(0.0, 0.0, 0.0);
      unsigned int three_j;

      templateVertex(0) = vertsTemplate->at(_templateIndex * 3);
      templateVertex(1) = vertsTemplate->at(_templateIndex * 3 + 1);
      templateVertex(2) = vertsTemplate->at(_templateIndex * 3 + 2);

      for(unsigned int j=_targetStart; j<=_targetEnd; ++j)
      {
       three_j = 3*j;
       targetVertex(0) = vertsTarget->at(three_j);
       targetVertex(1) = vertsTarget->at(three_j + 1);
       targetVertex(2) = vertsTarget->at(three_j + 2);

       distance = euclideanDistance(templateVertex, targetVertex);
       if(distance < minDistance)
        {
         auxUi(0) = targetVertex(0);
         auxUi(1) = targetVertex(1);
         auxUi(2) = targetVertex(2);
         minDistance = distance;
         targetIndex = j;
         foundCorrespondence = true;
        }
       }

        if(foundCorrespondence)
        {
          Vector3f templateNormal = m_template->getNormal(_templateIndex);
          Vector3f targetNormal = m_target->getNormal(targetIndex);
          float normalDot = templateNormal.dot(targetNormal);

          if(normalDot >= 0.0 && normalDot <= 1.0)
          {
           (*m_U)(_templateIndex, 0) = auxUi(0);
           (*m_U)(_templateIndex, 1) = auxUi(1);
           (*m_U)(_templateIndex, 2) = auxUi(2);
          }
          else
          {
           (*m_W)(_templateIndex) = 0.0;
          }
        }
        else
        {
          (*m_W)(_templateIndex) = 0.0;
        }
  }


void NRICP::determineOptimalDeformation()
  {
    //Find X = (At*A)-1 * At * B
    //For current stiffness alpha

    float auxValue = 0.0;
    unsigned int four_i;
    unsigned int four_j;
    unsigned int auxRowIndex;
    int weight;
    unsigned short vertPerEdge;

    for(unsigned int i=0; i<m_templateEdgeCount; ++i)
     {
       four_i = 4*i;
       vertPerEdge = 0;
       for(unsigned int j=0; j<m_templateVertCount && vertPerEdge <= 2; ++j)
       {
        auxValue = m_stiffness * (m_M->coeff(i, j));

        if(auxValue > 0.0 || auxValue < 0.0)
        {
         four_j = 4*j;
         vertPerEdge ++;
         m_A->coeffRef(four_i,four_j) = auxValue;
         m_A->coeffRef(four_i + 1,four_j + 1) = auxValue;
         m_A->coeffRef(four_i + 2,four_j + 2) = auxValue;
         m_A->coeffRef(four_i + 3,four_j + 3) = auxValue * m_gamma;
        }
       }
     }


    for(unsigned int i = 0; i < m_templateVertCount; ++i)
    {
        auxRowIndex = i + 4 * m_templateEdgeCount;
        four_i = 4 * i;
        weight = (*m_W)(i);

        if(weight > 0)
        {
         m_A->coeffRef(auxRowIndex, four_i) = m_D->coeff(i, four_i) * weight;
         m_A->coeffRef(auxRowIndex, four_i + 1) = m_D->coeff(i, four_i + 1) * weight;
         m_A->coeffRef(auxRowIndex, four_i + 2) = m_D->coeff(i, four_i + 2) * weight;
         m_A->coeffRef(auxRowIndex, four_i + 3) = m_D->coeff(i, four_i + 3) * weight;

         m_B->coeffRef(auxRowIndex, 0) = (*m_U)(i, 0) * weight;
         m_B->coeffRef(auxRowIndex, 1) = (*m_U)(i, 1) * weight;
         m_B->coeffRef(auxRowIndex, 2) = (*m_U)(i, 2) * weight;
        }
    }

    m_A->makeCompressed();
    m_B->makeCompressed();

    SparseLU <SparseMatrix<GLfloat> > solver;
    solver.compute((*m_A).transpose() * (*m_A));
    SparseMatrix<GLfloat> I(4 * m_templateVertCount,4 * m_templateVertCount);
    I.setIdentity();
    SparseMatrix<GLfloat> A_inv = solver.solve(I);
    SparseMatrix<GLfloat> result = A_inv * (*m_A).transpose() * (*m_B);
    result.uncompress();
    (*m_X) = result;
}


void NRICP::deformTemplate()
  {
      unsigned int four_i;
      MatrixXf auxMultiplication(m_templateVertCount, 3);
      auxMultiplication = (*m_D) * (*m_X);


      //Change point values in m_D, which will change them in the mesh
      //Change points in the mesh
      for(unsigned int i = 0; i < m_templateVertCount; ++i)
      {
        four_i = 4 * i;
        m_D->coeffRef(i, four_i) = auxMultiplication(i, 0);
        m_D->coeffRef(i, four_i + 1) = auxMultiplication(i, 1);
        m_D->coeffRef(i, four_i + 2) = auxMultiplication(i, 2);
      }

      m_template->changeVertsBasedOn_D_Matrix();
      m_D->makeCompressed();
  }


//Auxiliary
float NRICP::euclideanDistance(Vector3f _v1, Vector3f _v2)
  {
      float diff1 = _v1(0) - _v2(0);
      float diff2 = _v1(1) - _v2(1);
      float diff3 = _v1(2) - _v2(2);

      return sqrt(diff1*diff1 + diff2*diff2 + diff3*diff3);
  }




