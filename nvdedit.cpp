// редактор раздела nvdload
#include "nvdedit.h"
#include "MainWindow.h"
#include <string.h>
#include "ptable.h"
#include "viewer.h"
#include "nvexplorer.h"

//********************************************************************
//* Конструктор класса
//********************************************************************
nvdedit::nvdedit(int xpnum, QWidget* parent) : QWidget(parent) {

QString str;  
QFont font;
QFont oldfont;
QFont labelfont;

pnum=xpnum;

// Локальная копия данных раздела
data=new uint8_t[ptable->psize(pnum)];
plen=ptable->psize(pnum);
memcpy(data,ptable->iptr(pnum),plen);

// заголовок раздела
memcpy(&hdr,data,sizeof(hdr));

// тип файла
if (hdr.nv_bin.off == sizeof(hdr)) filetype=2; // полный заголовок - новые чипсеты
   else filetype=1;   // частичный заголовок - старые чипсеты
    
// компоненты раздела
//  nv.bin
nvpart=new uint8_t[hdr.nv_bin.len];
memcpy(nvpart,data+hdr.nv_bin.off,hdr.nv_bin.len);
// основной xml
if ((hdr.xnv_xml).magic == NV_FILE_MAGIC) {
 xmlpart=new uint8_t[hdr.xnv_xml.len];
 memcpy(xmlpart,data+hdr.xnv_xml.off,hdr.xnv_xml.len);
}
// дополнительный xml
if (hdr.cust_xml.magic == NV_FILE_MAGIC) {
 custxmlpart=new uint8_t[hdr.cust_xml.len];
 memcpy(custxmlpart,data+hdr.cust_xml.off,hdr.cust_xml.len);
}
// xml map
if (hdr.xnv_map.magic == NV_FILE_MAGIC) {
 xmlmap=new uint8_t[hdr.xnv_map.len];
 memcpy(xmlmap,data+hdr.xnv_map.off,hdr.xnv_map.len);
}


// Вертикальный компоновщик
vlm=new QVBoxLayout(this);

// Вынимаем текущие параметры шрифта меток 
font=QApplication::font("QLabel");
oldfont=font;

// Заголовок панели
font.setPointSize(font.pointSize()+7);
font.setBold(true);
hdrlabel=new QLabel("NVDLOAD section editor",this);
hdrlabel->setFont(font);
hdrlabel->setStyleSheet("QLabel { color : green; }");
vlm->addWidget(hdrlabel,0,Qt::AlignHCenter);

// Увеличиваем шрифт по умолчанию на 2 пункта
labelfont=oldfont;
labelfont.setPointSize(labelfont.pointSize()+2);

// Type de fichier
if (filetype == 1) str = "NVDLOAD structure type: 1 (chipset V7R11 and older)";
else str = "NVDLOAD structure type: 2 (V7R22 chipset and newer)";
hdrlabel=new QLabel(str,this);
hdrlabel->setFont(labelfont);
hdrlabel->setStyleSheet("QLabel { color : blue; }");
vlm->addWidget(hdrlabel);
vlm->addStretch(1);

// Компоновщик списка компонентов
lcomp=new QGridLayout(0);
lcomp->setVerticalSpacing(15);
vlm->addLayout(lcomp);

// заголовок таблицы
font=oldfont;
font.setPointSize(font.pointSize()+3);
font.setBold(true);

comphdr1=new QLabel("Composant  ",this);
comphdr1->setFont(font);
comphdr1->setStyleSheet("QLabel { color : red; }");
lcomp->addWidget(comphdr1,0,0);

comphdr2=new QLabel("The size ", this);
comphdr2->setFont(font);
comphdr2->setStyleSheet("QLabel { color : orange; }");
lcomp->addWidget(comphdr2,0,1);

comphdr3=new QLabel("The teams",this);
comphdr3->setFont(font);
comphdr3->setStyleSheet("QLabel { color : green; }");
lcomp->addWidget(comphdr3,0,2,1,2,Qt::AlignHCenter);

// имена компонентов
name1=new QLabel("NVIMG",this);
name1->setFont(labelfont);
lcomp->addWidget(name1,1,0);

name2=new QLabel("Base XML",this);
name2->setFont(labelfont);
lcomp->addWidget(name2,2,0);

name3=new QLabel("Custom XML",this);
name3->setFont(labelfont);
lcomp->addWidget(name3,3,0);

name4=new QLabel("XML MAP",this);
name4->setFont(labelfont);
lcomp->addWidget(name4,4,0);

// размеры компонентов
str.sprintf("%i",hdr.nv_bin.len);
size1=new QLabel(str,this);
size1->setFont(labelfont);
lcomp->addWidget(size1,1,1,Qt::AlignHCenter);

str.sprintf("%i",hdr.xnv_xml.len);
size2=new QLabel(str,this);
size2->setFont(labelfont);
lcomp->addWidget(size2,2,1,Qt::AlignHCenter);

str.sprintf("%i",hdr.cust_xml.len);
size3=new QLabel(str,this);
size3->setFont(labelfont);
lcomp->addWidget(size3,3,1,Qt::AlignHCenter);

str.sprintf("%i",hdr.xnv_map.len);
size3=new QLabel(str,this);
size3->setFont(labelfont);
lcomp->addWidget(size3,4,1,Qt::AlignHCenter);


// кнопки извлечения 
extr1=new QPushButton("Exit",this);
connect(extr1,SIGNAL(clicked()),this,SLOT(extract1()));
lcomp->addWidget(extr1,1,2);

if (hdr.xnv_xml.len != 0) {
 extr2=new QPushButton("Exit",this);
 connect(extr2,SIGNAL(clicked()),this,SLOT(extract2()));
 lcomp->addWidget(extr2,2,2);
}

if (hdr.cust_xml.len != 0) {
 extr3=new QPushButton("Exit",this);
 connect(extr3,SIGNAL(clicked()),this,SLOT(extract3()));
 lcomp->addWidget(extr3,3,2);
}

if (hdr.xnv_map.len != 0) {
 extr4=new QPushButton("Exit",this);
 connect(extr4,SIGNAL(clicked()),this,SLOT(extract4()));
 lcomp->addWidget(extr4,4,2);
}

// кнопки замены
repl1=new QPushButton("Replace",this);
connect(repl1,SIGNAL(clicked()),this,SLOT(replace1()));
lcomp->addWidget(repl1,1,3);

if (hdr.xnv_xml.len != 0) {
 repl2=new QPushButton("Replace",this);
 connect(repl2,SIGNAL(clicked()),this,SLOT(replace2()));
 lcomp->addWidget(repl2,2,3);
}

if (hdr.cust_xml.len != 0) {
 repl3=new QPushButton("Replace",this);
 connect(repl3,SIGNAL(clicked()),this,SLOT(replace3()));
 lcomp->addWidget(repl3,3,3);
}

if (hdr.xnv_map.len != 0) {
 repl4=new QPushButton("Replace",this);
 connect(repl4,SIGNAL(clicked()),this,SLOT(replace4()));
 lcomp->addWidget(repl4,4,3);
}

// кнопки редактирования

edit1=new QPushButton("To edit",this);
connect(edit1,SIGNAL(clicked()),this,SLOT(nvexpl()));
lcomp->addWidget(edit1,1,4);


if (hdr.xnv_xml.len != 0) {
 edit2=new QPushButton("To edit",this);
 connect(edit2,SIGNAL(clicked()),this,SLOT(xedit2()));
 lcomp->addWidget(edit2,2,4);
}

if (hdr.cust_xml.len != 0) {
 edit3=new QPushButton("To edit",this);
 connect(edit3,SIGNAL(clicked()),this,SLOT(xedit3()));
 lcomp->addWidget(edit3,3,4);
}

// правая распорка
rspacer=new QSpacerItem(100,10,QSizePolicy::Expanding);
lcomp->addItem(rspacer,1,5);

vlm->addStretch(7);
}

//********************************************************************
//* Деструктор класса
//********************************************************************
nvdedit::~nvdedit() {

QMessageBox::StandardButton reply;
QString cmd;
 
// пересобираем данные
if (changed) rebuild_data();

// проверяем, изменились ли данные
if ((ptable->psize(pnum) != plen) || (memcmp(data,ptable->iptr(pnum),plen) != 0)) {
  reply=QMessageBox::warning(this,"Section recording ", " Content changed, save?",QMessageBox::Ok | QMessageBox::Cancel);
  if (reply == QMessageBox::Ok) {
    ptable->replace(pnum,data,plen);
  }
}  
delete [] data;
delete nvpart;
if (xmlpart != 0) delete [] xmlpart;
if (custxmlpart != 0) delete [] custxmlpart;
if (xmlmap != 0) delete [] xmlmap;

}

//********************************************************************
//* Извлечение компонентов
//*   0 - NVIMG
//*   1 - Base XML
//*   2 - Custom XML
//*   3 - XNV MAP
//********************************************************************
void nvdedit::extractor(int type) {

// имена файлов по умолчанию
char* compnames[4]= {
  "nvimg.nvm",
  "base.xml",
  "custom.xml",
  "xnvmap.bin"
};  
uint32_t start=0,len=0;

QString filename=compnames[type];

switch(type) {
  case 0:
    start=hdr.nv_bin.off;
    len=hdr.nv_bin.len;
    break;
    
  case 1:
    start=hdr.xnv_xml.off;
    len=hdr.xnv_xml.len;
    break;
    
  case 2:
    start=hdr.cust_xml.off;
    len=hdr.cust_xml.len;
    break;
    
  case 3:
    start=hdr.xnv_map.off;
    len=hdr.xnv_map.len;
    break;
}   

filename=QFileDialog::getSaveFileName(this,"Stored file name",filename,"All files (*)");
if (filename.isEmpty()) return;

QFile out(filename,this);
if (!out.open(QIODevice::WriteOnly)) {
    QMessageBox::critical(0, "Error", "Error while creating file");
    return;
}
out.write((char*)(data+start),len);
out.close();
}


//********************************************************************
//* Слоты для извлечения образов компонентов
//********************************************************************
void nvdedit::extract1() { extractor(0); }
void nvdedit::extract2() { extractor(1); }
void nvdedit::extract3() { extractor(2); }
void nvdedit::extract4() { extractor(3); }


//********************************************************************
//* Замена компонентов
//*   0 - NVIMG
//*   1 - Base XML
//*   2 - Custom XML
//*   3 - XNV MAP
//********************************************************************
void nvdedit::replacer(int type) {

QString filename="";
uint32_t fsize;

// выбор файла
filename=QFileDialog::getOpenFileName(this,"File name",filename,"All files (*)");
if (filename.isEmpty()) return;

QFile out(filename,this);
if (!out.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(0, "Error", "Error reading file");
    return;
}

// Читаем образ компонента из файла
fsize=out.size();
uint8_t* fbuf=new uint8_t[fsize]; // файловый буфер
bzero(fbuf,fsize);
out.read((char*)fbuf,fsize);
out.close();

// устанавливаем указатель на фвйловый буфер, старые данные херим
switch(type) {
  case 0:
    delete nvpart;
    nvpart=fbuf;
    hdr.nv_bin.len=fsize;
    break;
    
  case 1:
    delete xmlpart;
    xmlpart=fbuf;
    hdr.xnv_xml.len=fsize;
    break;
    
  case 2:
    delete custxmlpart;
    custxmlpart=fbuf;
    hdr.cust_xml.len=fsize;
    break;
    
  case 3:
    delete xmlmap;
    xmlmap=fbuf;
    hdr.xnv_map.len=fsize;
    break;
}   
// Пересоздаем область данных
rebuild_data();

}

//********************************************************************
//* Слоты для замены образов компонентов
//********************************************************************
void nvdedit::replace1() { replacer(0); }
void nvdedit::replace2() { replacer(1); }
void nvdedit::replace3() { replacer(2); }
void nvdedit::replace4() { replacer(3); }


//********************************************************************
//* Слот для редактирования двоичной NV-базы данных
//********************************************************************
void nvdedit::nvexpl() {

nvexplorer* exp=new nvexplorer(data+hdr.nv_bin.off,hdr.nv_bin.len);
exp->show();
}
  

//********************************************************************
//* Редактор XML-компонентов
//********************************************************************
void nvdedit::xeditor(int pn) {
 
viewer* viewpanel; 
  
switch(pn) {
  case 1:
    viewpanel=new viewer(xmlpart,&hdr.xnv_xml.len,0,"Base XML component");
    break;

  case 2:  
    viewpanel=new viewer(custxmlpart,&hdr.cust_xml.len,0,"Base XML component");
    break;

  default:
    viewpanel=0;
    return;
}

connect(viewpanel,SIGNAL(changed()),this,SLOT(setchanged()));
}  


//********************************************************************
//* Слоты для редактирования XML-компонентов
//********************************************************************
void nvdedit::xedit2() { xeditor(1); }
void nvdedit::xedit3() { xeditor(2); }



//********************************************************************
//* Пересборка области данных
//********************************************************************
void nvdedit::rebuild_data() {

uint32_t off;
uint32_t hdsize;
uint32_t totalsize;
uint8_t* newdata;

// размер заголовка
if (filetype == 1) hdsize=7*sizeof(struct nv_file_info);
else hdsize=sizeof(nv_dload_packet_head);
  
// Вычисляем новый размер раздела
totalsize=hdr.nv_bin.len+hdr.xnv_xml.len+hdr.cust_xml.len+hdr.xnv_map.len;

// Выделяем память под новый образ раздела (образы частей + заголовок + 4 байта чексуммы)
newdata=new uint8_t[hdsize+totalsize+4];

// настраиваем указатели источника-приемника
off=hdsize;

// копируем разделы

if (hdr.nv_bin.len != 0) {
 hdr.nv_bin.off=off;
 memcpy(newdata+off,nvpart,hdr.nv_bin.len);
 off+=hdr.nv_bin.len;
}

if (hdr.xnv_xml.len != 0) {
  hdr.xnv_xml.off=off;
  memcpy(newdata+off,xmlpart,hdr.xnv_xml.len);
  off+=hdr.xnv_xml.len;
}  

if (hdr.cust_xml.len != 0) {
  hdr.cust_xml.off=off;
  memcpy(newdata+off,custxmlpart,hdr.cust_xml.len);
  off+=hdr.cust_xml.len;
}  

if (hdr.xnv_map.len != 0) {
  hdr.xnv_map.off=off;
  memcpy(newdata+off,xmlmap,hdr.xnv_map.len);
//   off+=hdr.xnv_map.len;
}  
// копируем заголовок
memcpy(newdata,&hdr,hdsize);

// подставляем новый размер вместо старого
plen=totalsize+hdsize+4;

// Копируем старую КС. Пока вычислять ее я не умею, да она и не нужна
memcpy(newdata+plen-4,data+plen-4,4);

// Подставляем новый буфер данных вместо старого 
delete data;
data=newdata;
}

