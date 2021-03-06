#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 100;
static int viewy = 100;
static int viewz = 240;

float rot = 0;

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
}


//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

void awan()
{

glPushMatrix();
glColor3ub(153, 223, 255);
glutSolidSphere(10, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(10,0,1);
glutSolidSphere(5, 20, 20);
glPopMatrix();
glPushMatrix();
glTranslatef(-2,6,-2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(-10,-3,0);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(6,-2,2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glEndList(); //untuk menutup glnewlist
}

void awan2()
{

glPushMatrix();
glColor3ub(153, 223, 255);
glutSolidSphere(10, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(10,0,1);
glutSolidSphere(5, 20, 20);
glPopMatrix();
glPushMatrix();
glTranslatef(-2,6,-2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(-10,-3,0);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(6,-2,2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glEndList(); //untuk menutup glnewlist
}

void awan3()
{

glPushMatrix();
glColor3ub(153, 223, 255);
glutSolidSphere(10, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(10,0,1);
glutSolidSphere(5, 20, 20);
glPopMatrix();
glPushMatrix();
glTranslatef(-2,6,-2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(-10,-3,0);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(6,-2,2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glEndList(); 
//untuk menutup glnewlist
}
 
void matahari()
{

glPushMatrix();
glColor3ub(255, 253, 116);
glutSolidSphere(10, 60, 60);
glPopMatrix();
 
glEndList();
}


void meja()
{
	glPushMatrix();	
	glColor3ub(204,204,153);
	glTranslatef(0, 8, 0);
	glScalef(2, 0.2, 2);
	glutSolidCube(0.8);
	glPopMatrix();
	
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(0,7,0);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
   
}

void kursi()
{
	
	glPushMatrix();	
	glColor3ub(204,204,153);
	glTranslatef(0, 8, 2.7);
	glScalef(2, 2, 0.2);
	glutSolidCube(1.2);
	glRotatef(0,0,0,0);
	glPopMatrix();
	
	glPushMatrix();	
	glColor3ub(204,204,153);
	glTranslatef(0, 7, 0);
	glScalef(2, 0.2, 4);
	glutSolidCube(1.4);
	glPopMatrix();
	
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(-1,5.9,2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(-1,5.9,-2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(1,5.9,2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(1,5.9,-2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
}


void kursi2()
{
	
	glPushMatrix();	
	glColor3ub(204,204,153);
	glTranslatef(0, 8, 2.7);
	glScalef(2, 2, 0.2);
	glutSolidCube(1.2);
	glRotatef(0,0,0,0);
	glPopMatrix();
	
	glPushMatrix();	
	glColor3ub(204,204,153);
	glTranslatef(0, 7, 0);
	glScalef(2, 0.2, 4);
	glutSolidCube(1.4);
	glPopMatrix();
	
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(-1,5.9,2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(-1,5.9,-2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
      glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(1,5.9,2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
   
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(1,5.9,-2);
   glScalef(0.1,2.5,0.1);
   glutSolidCube(1.0);
   glPopMatrix();
}

void lapanganvoli()
{
     GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH);  

   	glPushMatrix();	
	glColor3ub(204,204,153);
	glTranslatef(0, 27, -38);
	glScalef(0.2, 3, 14);
	glutSolidCube(4);
	glRotatef(120,0,0,100);
	glPopMatrix();
	
	glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(0,7,-10);
   glRotatef(260,1,0,0);
   gluCylinder(pObj, 1, 0.5, 25, 25, 25);
   glPopMatrix();              
             
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(0,7,-60);
   glRotatef(260,1,0,0);
   gluCylinder(pObj, 1, 0.5, 25, 25, 25);
   glPopMatrix();      
}

void payung()
{
     GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH);  
	glPushMatrix();	
	glColor3ub(204,100,153);
	glTranslatef(0, 32, 0);
	glScalef(15, 0.2, 15);
	glutSolidSphere(1, 20, 6);
	glPopMatrix();
	
   glPushMatrix();
   glColor3ub(204,204,153);
   glTranslatef(0,7,0);
   glRotatef(270,1,0,0);
   gluCylinder(pObj, 1, 0.5, 25, 25, 25);
  
   glPopMatrix();
   
}

void batu()
{

glPushMatrix();
glColor3ub(112,112,112);
glutSolidSphere(40, 100, 50);
glPopMatrix();

glEndList();
}



unsigned int LoadTextureFromBmpFile(char *filename);

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0);
	glClearColor(0.0, 0.8, 0.9, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();
	drawSceneTanah(_terrain, 0.8, 0.5, 0.2);
	glPopMatrix();

	glPushMatrix();
	drawSceneTanah(_terrainTanah, 0.7f, 0.2f, 0.1f);
	glPopMatrix();

	glPushMatrix();
	drawSceneTanah(_terrainAir, 0.0f, 0.4f, 0.7f);
	glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-30,50,-130);
    awan();
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-140,65,-150);
    glScalef(2.0,2.0,2.0);
    awan2();
    
    glPushMatrix();
    glTranslatef(-120,165,-100);
    glScalef(2.0,2.0,2.0);
    awan3();
    
    
    
    glPushMatrix();
    glTranslatef(-32,-35.2,115);    
    glScalef(5, 5, 5);
    meja();
    glPopMatrix();
    
    
    glPushMatrix();
    glTranslatef(-20,-35.2,105);    
    glScalef(5, 5, 5);
    kursi();
    glPopMatrix();
    
    
    glPushMatrix();
    glTranslatef(-45,-35.2,105);    
    glScalef(5, 5, 5);
    kursi2();
    glPopMatrix();
    
    
    glPushMatrix();
    glTranslatef(120,-12,135);    
    glScalef(1, 1, 1);
    lapanganvoli();
    glPopMatrix();
    
    
    glPushMatrix();
    glTranslatef(-30,-12,110);    
    glScalef(1, 1, 1);
    payung();
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(70,-10.,160); 
    glScalef(0.11,0.11,0.1);   
    batu();
    glPopMatrix();
    
    
    glPushMatrix();
    glTranslatef(-25,65,-140);    
    matahari();
    glPopMatrix();
  
    
     
	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

}

void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);

	//binding texture

}

static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Sample Terain");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 0;
}
