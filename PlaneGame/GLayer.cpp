
#include "GLayer.h"
#include "GClipmap.h"
#include "GPixel.h"
#include "GHeight.h"
#include "CHgtFile.h"


#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <math.h>
#include <chrono>


GLayer::GLayer(GClipmap* clipmapPointer, float inDegree, float inHgtFileDegree, float inScale, int inLayerIndex, 
               int inHgtSkipping, int inHgtFileResolution, int inRawSkipping, int inRawFileResolution, int inN) : clipmap(clipmapPointer){

    readDegree = inDegree;
    scale = inScale;
    layerIndex = inLayerIndex;
    n = inN;

    firstGothrough = true;
    

    hgtTextureBegginingXBuff = 0;  //x
    hgtTextureBegginingYBuff = n;  //y

    rawTextureBegginingXBuff = 0;  //x
    rawTextureBegginingYBuff = n - 1;  //y

    allFinerHorizontalSum = 0;
    allFinerVerticalSum = 0;

    program = clipmapPointer->program;
    vaoA = clipmapPointer->vaoA;
    vaoB = clipmapPointer->vaoB;
    vaoC = clipmapPointer->vaoC;
    vaoD = clipmapPointer->vaoD;
    vaoE = clipmapPointer->vaoE;
    vaoF = clipmapPointer->vaoF;

    indexA_Buffer = clipmapPointer->indexA_Buffer;
    indexB_Buffer = clipmapPointer->indexB_Buffer;
    indexC_Buffer = clipmapPointer->indexC_Buffer;
    indexD_Buffer = clipmapPointer->indexD_Buffer;
    indexE_Buffer = clipmapPointer->indexE_Buffer;
    indexF_Buffer = clipmapPointer->indexF_Buffer;


    drawMode = &(clipmapPointer->drawingMode);
    pixelManager = new GPixel(this, inLayerIndex, inDegree, inRawSkipping, inRawFileResolution);
    heightManager = new GHeight(this, inLayerIndex, inDegree, inHgtSkipping, inHgtFileResolution, inHgtFileDegree);

    performance = CPerformance::getInstance();

    cumputeOffsets();
    
}

void GLayer::buildLayer() {
    
    int m = (n + 1) / 4 - 1;
   
    //setting shader variables
    program->setUniformValue("worldOffset", offsets);                      //placing layer in world coordinates   
    program->setUniformValue("levelScaleFactor", QVector2D(scale, scale)); //setting right scale 
    program->setUniformValue("levelIndex", layerIndex);                    //setting right index
  
    program->setUniformValue("rawTexOffset", QVector2D(rawTextureBegginingX, rawTextureBegginingY));
    program->setUniformValue("hgtTexOffset", QVector2D(hgtTextureBegginingX, hgtTextureBegginingY));

    if (clipmap->highestLvlOfDetail > layerIndex) {


        program->setUniformValue("rawTexOffsetFnr", QVector2D(clipmap->layer[layerIndex + 1].rawTextureBegginingX, clipmap->layer[layerIndex + 1].rawTextureBegginingY));
        program->setUniformValue("hgtTexOffsetFnr", QVector2D(clipmap->layer[layerIndex + 1].hgtTextureBegginingX, clipmap->layer[layerIndex + 1].hgtTextureBegginingY));
        program->setUniformValue("cameraPosition", QVector2D( (horizontalOffset + allFinerHorizontalSum), (verticalOffset + allFinerVerticalSum)));
        program->setUniformValue("highestLvlOfDetail", clipmap->highestLvlOfDetail);

        if (clipmap->layer[layerIndex + 1].positionVertical == 0) { 
            if (clipmap->layer[layerIndex + 1].positionHorizontal == 0) 
                program->setUniformValue("coarserInFinerOffset", QVector2D(m, m));
            else if (clipmap->layer[layerIndex + 1].positionHorizontal == 1)
                program->setUniformValue("coarserInFinerOffset", QVector2D(m+1,m));
        }
        else if (clipmap->layer[layerIndex + 1].positionVertical == 1) {
            if (clipmap->layer[layerIndex + 1].positionHorizontal == 0) 
                program->setUniformValue("coarserInFinerOffset", QVector2D(m,m+1));
            else if (clipmap->layer[layerIndex + 1].positionHorizontal == 1) 
                program->setUniformValue("coarserInFinerOffset", QVector2D(m+1,m+1));
        }
    }
     

    drawA(0.0,    0.0);
    drawA(m,      0.0);
    drawB(2*m,    0.0);
    drawA(2*m+2,  0.0);
    drawA(3*m+2,  0.0);
    drawA(3*m+2,  m);
    drawC(3*m+2,  2*m);
    drawA(3*m+2,  2*m+2);
    drawA(3*m+2,  3*m+2);
    drawA(2*m+2,  3*m+2);
    drawB(2*m,    3*m+2);
    drawA(m,      3*m+2);
    drawA(0.0,    3*m+2);
    drawA(0.0,    2*m+2);
    drawC(0.0,    2*m);
    drawA(0.0,    m);

    if (clipmap->activeLvlOfDetail == layerIndex) {
        drawA(m,        m);
        drawB(2*m,      m);
        drawA(2*m+2,    m);
        drawC(2*m+2,    2*m);
        drawA(2*m+2,    2*m+2);
        drawB(2*m,      2*m+2);
        drawA(m,        2*m+2);
        drawC(m,        2*m);
        drawF(2*m,      2*m);
    }
    else {

        if (clipmap->layer[layerIndex - 1].positionVertical == 0) { //down wall
            
            drawD(m, 3*m+1);

            if (clipmap->layer[layerIndex - 1].positionHorizontal == 0) { //left wall
                drawE(3*m+1, m+1);
            }
            else if (clipmap->layer[layerIndex - 1].positionHorizontal == 1) { //right wall
                drawE(m, m+1);
            }

        }
        else if (clipmap->layer[layerIndex - 1].positionVertical == 1) { //top wall

            drawD(m, m);
            
            if (clipmap->layer[layerIndex - 1].positionHorizontal == 0) { //left wall
                drawE(3*m+1, m);
            }
            else if (clipmap->layer[layerIndex - 1].positionHorizontal == 1) { //right wall
                drawE(m, m);
            }
        }

    }

}




void GLayer::updateLayer(double tlon, double tlat) {
    
    double latDifference, lonDifference;

    //checking if there was a movement
    lonDifference = (-1) * (oldLon - tlon - fmod(oldLon - tlon, readDegree)) / readDegree;
    latDifference = (-1) * (oldLat - tlat -fmod(oldLat - tlat, readDegree)) / readDegree;

    //mapping data
    mapDataIntoImages(tlon, tlat, lonDifference, latDifference);

}


void GLayer::updateTextures() {
    
    //updating texture using new picture 
    pixelTexture = new QOpenGLTexture(*pixelManager->pixelMap, QOpenGLTexture::DontGenerateMipMaps);
    pixelTexture->bind(layerIndex, QOpenGLTexture::DontResetTextureUnit);

    heightTexture = new QOpenGLTexture(*heightManager->heightMap, QOpenGLTexture::DontGenerateMipMaps);
    heightTexture->bind(layerIndex+13);

    //program->setUniformValue(clipmap->pixelTextureLocation[layerIndex], layerIndex);
    program->setUniformValue(clipmap->pixelTextureLocation[layerIndex], layerIndex);
    program->setUniformValue(clipmap->heightTextureLocation[layerIndex], layerIndex+13);

}


void GLayer::mapDataIntoImages(double tlon, double tlat, int lonDifference, int latDifference) {
    
    double lonLeft, lonRight;
    double latTop, latDown;


    if (firstGothrough == true) {

        computeNewLonAndLat(tlon, tlat, &lonLeft, &lonRight, &latTop, &latDown);
        
        oldLon = tlon;
        oldLat = tlat;
        oldLonLeft = lonLeft;
        oldLonRight = lonRight;
        oldLatDown = latDown;
        oldLatTop = latTop;

    }


    if (firstGothrough == true || (abs(latDifference) > n - 1 || abs(lonDifference) > n - 1)) {

        //computing coordinates of corners of new moved clipmap 
        computeNewLonAndLat(tlon, tlat, &lonLeft, &lonRight, &latTop, &latDown);

        //updating pictures
        pixelManager->fullRawTextureReading(lonLeft, lonRight, latDown, latTop);
        heightManager->fullHgtTextureReading(lonLeft, lonRight, latDown, latTop);
           
        //reseting texture beggining coordinate
        hgtTextureBegginingXBuff = 0;
        hgtTextureBegginingYBuff = n;
        rawTextureBegginingXBuff = 0;  //x
        rawTextureBegginingYBuff = n - 1;  //y
            
        //updating old coordinates
        oldLat = tlat;
        oldLon = tlon;
        oldLatTop = latTop;
        oldLonRight = lonRight;
        oldLonLeft = lonLeft;
        oldLatDown = latDown;

        firstGothrough = false;    

        performance->trianglesRead += 2 * (n - 1) * (n - 1);
     

    }
    else if (lonDifference != 0 || latDifference != 0) {

        point texBegHor, texBegVer;
        
        //computing coordinates of corners of new moved clipmap 
        computeNewLonAndLat(tlon, tlat, &lonLeft, &lonRight, &latTop, &latDown);
        
        //checking texture begginings for horizontal and vertical reading based on movement case 
        computeTextureOffsets(latDifference, lonDifference, &rTexBegHor, &rTexBegVer, true);
        computeTextureOffsets(latDifference, lonDifference, &hTexBegHor, &hTexBegVer, false);


        if (latDifference != 0) {
            pixelManager->horizontalBlockRawTextureReading(lonDifference, latDifference, lonLeft, lonRight, latDown, latTop, oldLonLeft, oldLonRight, oldLatDown, oldLatTop, rTexBegHor);
            heightManager->horizontalBlockHgtTextureReading(lonDifference, latDifference, lonLeft, lonRight, latDown, latTop, oldLonLeft, oldLonRight, oldLatDown, oldLatTop, hTexBegHor);
        }
         
        if (lonDifference != 0) {
            pixelManager->verticalBlockRawTextureReading(lonDifference, latDifference, lonLeft, lonRight, latDown, latTop, oldLonLeft, oldLonRight, oldLatDown, oldLatTop, rTexBegVer);
            heightManager->verticalBlockHgtTextureReading(lonDifference, latDifference, lonLeft, lonRight, latDown, latTop, oldLonLeft, oldLonRight, oldLatDown, oldLatTop, hTexBegVer);
        }


        if (latDifference > 0) {
            rawTextureBegginingYBuff = (int)(rawTextureBegginingYBuff + latDifference) % (n - 1);
            hgtTextureBegginingYBuff = (int)(hgtTextureBegginingYBuff + latDifference) % (n);
        }
        if (latDifference < 0) {
            rawTextureBegginingYBuff = (int)(rawTextureBegginingYBuff + latDifference + n - 1) % (n - 1);
            hgtTextureBegginingYBuff = (int)(hgtTextureBegginingYBuff + latDifference + n) % (n);
        }
        if (lonDifference > 0) {
            rawTextureBegginingXBuff = (int)(rawTextureBegginingXBuff + lonDifference) % (n - 1);
            hgtTextureBegginingXBuff = (int)(hgtTextureBegginingXBuff + lonDifference) % (n);
        }
        if (lonDifference < 0) {
            rawTextureBegginingXBuff = (int)(rawTextureBegginingXBuff + lonDifference + n - 1) % (n - 1);
            hgtTextureBegginingXBuff = (int)(hgtTextureBegginingXBuff + lonDifference + n) % (n);
        }

        oldLat = oldLat + latDifference * readDegree;
        oldLon = oldLon + lonDifference * readDegree;
        oldLatTop = oldLatTop + latDifference * readDegree;
        oldLatDown = oldLatDown + latDifference * readDegree;
        oldLonLeft = oldLonLeft + lonDifference * readDegree;
        oldLonRight = oldLonRight + lonDifference * readDegree;
        
        //pixelManager->pixelMap->save("fotka.png");

        performance->trianglesRead += 2 * (abs(lonDifference) * (n - 1 - latDifference) ) + 2 * (abs(latDifference) * (n - 1));
        
    }

}



void GLayer::computeLayerPosition(double tlon, double tlat) {


    //adjusting layers in right place in relation to each other     
    allFinerHorizontalSum = 0;
    allFinerVerticalSum = 0;


    if (!layerIndex == 0) {


        if (clipmap->layer[layerIndex - 1].positionHorizontal == 0)         //left side
            allFinerHorizontalSum = clipmap->layer[layerIndex - 1].allFinerHorizontalSum / 2;
        else if (clipmap->layer[layerIndex - 1].positionHorizontal == 1)    //right side
            allFinerHorizontalSum = clipmap->layer[layerIndex - 1].allFinerHorizontalSum / 2 + 1;

        if (clipmap->layer[layerIndex - 1].positionVertical == 0)           //down side    
            allFinerVerticalSum = clipmap->layer[layerIndex - 1].allFinerVerticalSum / 2;
        else if (clipmap->layer[layerIndex - 1].positionVertical == 1)      //top side  
            allFinerVerticalSum = clipmap->layer[layerIndex - 1].allFinerVerticalSum / 2 + 1;

        offsets.setX(tlon - (horizontalOffset + allFinerHorizontalSum) * readDegree);
        offsets.setY(tlat - (verticalOffset + allFinerVerticalSum) * readDegree);

    }
    else {
        offsets.setX(tlon - horizontalOffset * readDegree);
        offsets.setY(tlat - verticalOffset * readDegree);
    }

    rawTextureBegginingX = rawTextureBegginingXBuff;
    rawTextureBegginingY = rawTextureBegginingYBuff;
    hgtTextureBegginingX = hgtTextureBegginingXBuff;
    hgtTextureBegginingY = hgtTextureBegginingYBuff;
    
}

void GLayer::cumputeOffsets() {

    if (layerIndex == 0) {
        horizontalOffset = (n - 1) / 2;
        verticalOffset = (n - 1) / 2;
    }
    else {

        double horOffHigherLayer = clipmap->layer[layerIndex - 1].horizontalOffset;
        double verOffHigherLayer = clipmap->layer[layerIndex - 1].verticalOffset;

        int m = (n + 1) / 4;

        horizontalOffset = (horOffHigherLayer / 2) + m - 1;
        verticalOffset = (verOffHigherLayer / 2) + m - 1;

    }
}

void GLayer::computeTextureOffsets(int latDifference, int lonDifference, point *texBegHor, point *texBegVer, bool rawReading) {

    if (rawReading) {
        if (latDifference > 0) {

            if (lonDifference > 0) {
                texBegHor->x = (rawTextureBegginingXBuff + lonDifference) % (n - 1);
                texBegHor->y = (rawTextureBegginingYBuff + latDifference) % (n - 1);
                texBegVer->x = rawTextureBegginingXBuff;
                texBegVer->y = rawTextureBegginingYBuff;
            }
            else if (lonDifference == 0) {
                texBegHor->x = rawTextureBegginingXBuff;
                texBegHor->y = (rawTextureBegginingYBuff + latDifference) % (n - 1);
            }
            else if (lonDifference < 0) {
                texBegHor->x = (rawTextureBegginingXBuff + lonDifference + n - 1) % (n - 1);
                texBegHor->y = (rawTextureBegginingYBuff + latDifference) % (n - 1);
                texBegVer->x = (rawTextureBegginingXBuff + lonDifference + n - 1) % (n - 1);
                texBegVer->y = rawTextureBegginingYBuff;
            }
        }
        else if (latDifference == 0) {

            if (lonDifference > 0) {
                texBegVer->x = rawTextureBegginingXBuff;
                texBegVer->y = rawTextureBegginingYBuff;
            }
            else if (lonDifference < 0) {
                texBegVer->x = (rawTextureBegginingXBuff + lonDifference + n - 1) % (n - 1);
                texBegVer->y = rawTextureBegginingYBuff;
            }
        }
        else if (latDifference < 0) {

            if (lonDifference > 0) {
                texBegHor->x = (rawTextureBegginingXBuff + lonDifference) % (n - 1);
                texBegHor->y = rawTextureBegginingYBuff;
                texBegVer->x = rawTextureBegginingXBuff;
                texBegVer->y = (rawTextureBegginingYBuff + latDifference + n - 1) % (n - 1);
            }
            else if (lonDifference == 0) {
                texBegHor->x = rawTextureBegginingXBuff;
                texBegHor->y = rawTextureBegginingYBuff;
            }
            else if (lonDifference < 0) {
                texBegHor->x = (rawTextureBegginingXBuff + lonDifference + n - 1) % (n - 1);
                texBegHor->y = rawTextureBegginingYBuff;
                texBegVer->x = (rawTextureBegginingXBuff + lonDifference + n - 1) % (n - 1);
                texBegVer->y = (rawTextureBegginingYBuff + latDifference + n - 1) % (n - 1);
            }
        }
    }
    else {
        if (latDifference > 0) {

            if (lonDifference > 0) {
                texBegHor->x = (hgtTextureBegginingXBuff + lonDifference) % (n);
                texBegHor->y = (hgtTextureBegginingYBuff + latDifference) % (n);
                texBegVer->x = hgtTextureBegginingXBuff;
                texBegVer->y = hgtTextureBegginingYBuff;
            }
            else if (lonDifference == 0) {
                texBegHor->x = hgtTextureBegginingXBuff;
                texBegHor->y = (hgtTextureBegginingYBuff + latDifference) % (n);
            }
            else if (lonDifference < 0) {
                texBegHor->x = (hgtTextureBegginingXBuff + lonDifference + n) % (n);
                texBegHor->y = (hgtTextureBegginingYBuff + latDifference) % (n);
                texBegVer->x = (hgtTextureBegginingXBuff + lonDifference + n) % (n);
                texBegVer->y = hgtTextureBegginingYBuff;
            }
        }
        else if (latDifference == 0) {

            if (lonDifference > 0) {
                texBegVer->x = hgtTextureBegginingXBuff;
                texBegVer->y = hgtTextureBegginingYBuff;
            }
            else if (lonDifference < 0) {
                texBegVer->x = (hgtTextureBegginingXBuff + lonDifference + n) % (n);
                texBegVer->y = hgtTextureBegginingYBuff;
            }
        }
        else if (latDifference < 0) {

            if (lonDifference > 0) {
                texBegHor->x = (hgtTextureBegginingXBuff + lonDifference) % (n);
                texBegHor->y = hgtTextureBegginingYBuff;
                texBegVer->x = hgtTextureBegginingXBuff;
                texBegVer->y = (hgtTextureBegginingYBuff + latDifference + n) % (n);
            }
            else if (lonDifference == 0) {
                texBegHor->x = hgtTextureBegginingXBuff;
                texBegHor->y = hgtTextureBegginingYBuff;
            }
            else if (lonDifference < 0) {
                texBegHor->x = (hgtTextureBegginingXBuff + lonDifference + n) % (n);
                texBegHor->y = hgtTextureBegginingYBuff;
                texBegVer->x = (hgtTextureBegginingXBuff + lonDifference + n) % (n);
                texBegVer->y = (hgtTextureBegginingYBuff + latDifference + n) % (n);
            }
        }
    }
}

void GLayer::computeNewLonAndLat(double tlon, double tlat, double *lonLeft, double *lonRight, double *latTop, double *latDown) {

    *lonLeft = tlon - horizontalOffset * readDegree; //Left Right
    *lonRight = *lonLeft + (n - 1) * readDegree;
    *latTop = tlat + verticalOffset * readDegree;   //Top Down
    *latDown = *latTop - (n - 1) * readDegree;
}





void GLayer::drawA(float originX, float originY) {
    
    program->setUniformValue("patchOrigin", QVector2D(originX, originY));
    
    vaoA->bind();
    indexA_Buffer->bind();
    
    glDrawElements(*drawMode, clipmap->howManyToRenderA, GL_UNSIGNED_INT, 0);
    
    indexA_Buffer->release();
    vaoA->release();
}

void GLayer::drawB(float originX, float originY) {

    program->setUniformValue("patchOrigin", QVector2D(originX, originY));
   
    vaoB->bind();
    indexB_Buffer->bind();
    
    glDrawElements(*drawMode, clipmap->howManyToRenderB, GL_UNSIGNED_INT, 0);
    
    indexB_Buffer->release();
    vaoB->release();
}

void GLayer::drawC(float originX, float originY) {
    program->setUniformValue("patchOrigin", QVector2D(originX, originY));
    
    vaoC->bind();
    indexC_Buffer->bind();
    glDrawElements(*drawMode, clipmap->howManyToRenderC, GL_UNSIGNED_INT, 0);
    indexC_Buffer->release();
    vaoC->release();
}

void GLayer::drawD(float originX, float originY) {
    program->setUniformValue("patchOrigin", QVector2D(originX, originY));
    
    vaoD->bind();
    indexD_Buffer->bind();
    glDrawElements(*drawMode, clipmap->howManyToRenderD, GL_UNSIGNED_INT, 0);
    indexD_Buffer->release();
    vaoD->release();
}
 
void GLayer::drawE(float originX, float originY) {
    program->setUniformValue("patchOrigin", QVector2D(originX, originY));

    vaoE->bind();
    indexE_Buffer->bind();
    glDrawElements(*drawMode, clipmap->howManyToRenderE, GL_UNSIGNED_INT, 0);
    indexE_Buffer->release();
    vaoE->release();
}

void GLayer::drawF(float originX, float originY) {

    program->setUniformValue("patchOrigin", QVector2D(originX, originY));
    //program->setUniformValue("color", QVector3D(R, G, B));

    vaoB->bind();
    indexB_Buffer->bind();

    glDrawElements(*drawMode, 16, GL_UNSIGNED_INT, 0);

    indexB_Buffer->release();
    vaoB->release();
}