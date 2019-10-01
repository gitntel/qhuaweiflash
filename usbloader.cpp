#include <QtWidgets>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <termios.h>
#include <unistd.h>

#include "sio.h"
#include "usbloader.h"
#include "ulpatcher.h"

// указатель на открытый последовательный порт
extern int siofd; // fd для работы с Последовательным портом


//*************************************************
//* Рассчет контрольной суммы командного пакета
//*************************************************
void csum(unsigned char* buf, uint32_t len) {

unsigned  int i,c,csum=0;

unsigned int cconst[]={0,0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF};

for (i=0;i<len;i++) {
  c=(buf[i]&0xff);
  csum=((csum<<4)&0xffff)^cconst[(c>>4)^(csum>>12)];
  csum=((csum<<4)&0xffff)^cconst[(c&0xf)^(csum>>12)];
}  
buf[len]=(csum>>8)&0xff;
buf[len+1]=csum&0xff;
  
}

//*************************************************
//*   Отсылка командного пакета модему
//*************************************************
int sendcmd(void* srcbuf, int len) {

unsigned char replybuf[1024];
unsigned char cmdbuf[2048];
unsigned int replylen;

// локальная копия командного буфера
memcpy(cmdbuf,srcbuf,len);

// добавляем в нее контрольную сумму
csum(cmdbuf,len);

// отсылка команды
write(siofd,cmdbuf,len+2);  
tcdrain(siofd);

// читаем ответ
replylen=read(siofd,replybuf,1024);

if (replylen == 0) return 0;     // пустой ответ
if (replybuf[0] == 0xaa) return 1; // правильный ответ
return 0;
}

//*************************************
//* Поиск linux-ядра в образе раздела
//*************************************
int locate_kernel(uint8_t* pbuf, uint32_t size) {
  
int off;

for(off=(size-8);off>0;off--) {
  if (strncmp((char*)(pbuf+off),"ANDROID!",8) == 0) return off;
}
return 0;
}

//*********************************************
//* Поиск таблицы разделов в загрузчике 
//*********************************************
uint32_t find_ptable(uint8_t* buf, uint32_t size) {

// сигнатура заголовка таблицы  
const uint8_t headmagic[16]={0x70, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x48, 0x65, 0x61, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};  
uint32_t off;

for(off=0;off<(size-16);off+=4) {
  if (memcmp(buf+off,headmagic,16) == 0)   return off;
}
return 0;
}


//***************************************
//* Патч таблицы разделов
//***************************************
int ptable_patch(char* filename, uint8_t* pbuf[], struct lhead* part) {

FILE* in;
uint32_t fsize,ptoff;
char ptbuf[0x800];

in=fopen(filename,"r");
if (in == 0) {
  QMessageBox::critical(0, "Error", "Error opening the file");
  return 0;
}

// загружаем файл в буфер
fsize=fread(ptbuf,1,0x800,in);
fclose(in);
if (fsize != 0x800) {
  QMessageBox::critical(0, "Error", "File too short");
  return 0;
}  
if (strncmp((char*)ptbuf,"pTableHead",10) != 0) {
  QMessageBox::critical(0, "Error", "The file is not a partition table");
  return 0;
}  

// ищем таблицу разделов внутри загрузчика
ptoff=find_ptable(pbuf[1], part[1].size);
if (ptoff == 0) {
  QMessageBox::critical(0, "Error", "The built-in partition table is in the boot loader.");
  return 0;
}  
// замещаем таблицу разделов
memcpy(pbuf[1]+ptoff,ptbuf,0x800);
return 1;
}



//***************************************
//* Sélectionnez le fichier d'amorçage
//***************************************
void usbldialog::browse() {

QString name;  
name=QFileDialog::getOpenFileName(this, "Select the boot file",".","usbloader (*.bin);;All files (*)");
fname->setText(name);
}

//***************************************
//* Sélection d'un fichier de table de partition
//***************************************
void usbldialog::ptbrowse() {

QString name;  
name=QFileDialog::getOpenFileName(this, "Selecting a partition table file",".","usbloader (*.bin);;All files (*)");
ptfname->setText(name);
}

//***************************************
//* Очистка имени файла таблицы разделов
//***************************************
void usbldialog::ptclear() {

ptfname->setText("");
}

//***************************************
// fastboot-патч
//***************************************
int fastboot_only(uint8_t* pbuf[], struct lhead* part) {

int koff;  // смещение до ANDROID-заголовка

koff=locate_kernel(pbuf[1],part[1].size);
if (koff != 0) {
      *(pbuf[1]+koff)=0x55; // патч сигнатуры
      part[1].size=koff+8; // обрезаем раздел до начала ядра
      return 1;
}

QMessageBox::critical(0, "Error", "There is no ANDROID component in the loader - fast download is not possible");
return 0;
  
}


//***************************************
//* Отправка заголовка компонента
//***************************************
int start_part(uint32_t size,uint32_t adr,uint8_t lmode) {
  
struct __attribute__ ((__packed__)) {
  uint8_t cmd[3]={0xfe,0, 0xff};
  uint8_t lmode;
  uint32_t size;
  uint32_t adr;
}  cmdhead;

cmdhead.size=htonl(size);
cmdhead.adr=htonl(adr);
cmdhead.lmode=lmode;
  
return sendcmd(&cmdhead,sizeof(cmdhead));
}  
  
    
//***************************************
//* Отправка пакета данных
//***************************************
int send_data_packet(uint32_t pktcount, uint8_t* databuf, uint32_t datasize) {

// образ пакета
struct __attribute__ ((__packed__)) {
 uint8_t cmd=0xda; 
 uint8_t count;
 uint8_t rcount;
 uint8_t data[2048];
} cmddata;
  
cmddata.count=pktcount&0xff;
cmddata.rcount=(~pktcount)&0xff;
memcpy(cmddata.data,databuf,datasize);

return sendcmd(&cmddata,datasize+3);
}  


//***************************************
//* Закрытие потока данных компонента
//***************************************
int close_part(uint32_t pktcount) {

struct __attribute__ ((__packed__)) {
 uint8_t cmd=0xed;
 uint8_t count;
 uint8_t rcount;
} cmdeod; 
  
// Фрмируем пакет конца данных
cmdeod.count=pktcount&0xff;;
cmdeod.rcount=(~pktcount)&0xff;

return sendcmd(&cmdeod,sizeof(cmdeod));
}

//***************************************
//* Запуск загрузки
//***************************************
void usbload() {

// хранилище каталога компонентов загрузчика
struct lhead part[5];

// массив буферов для загрузки компонентов
uint8_t* pbuf[5]={0,0,0,0,0};

uint16_t numparts; // число компонентов для загрузки
  
uint32_t bl,datasize,pktcount;
uint32_t adr,i,fsize,totalsize=0,loadedsize=0;
uint8_t c;
int32_t res;
int32_t pflag,fflag,bflag;
// имена файлов - объявлены статическими и сохраняются при перезагрузке диалога
static char filename[200]={0};
static char ptfilename[200]={0};

FILE* in;

usbldialog* qd=new usbldialog;
qd->setWindowTitle("Loading usbloader ");
QVBoxLayout* vl=new QVBoxLayout(qd);

QFont font;
font.setPointSize(17);
font.setBold(true);
font.setWeight(75);

QLabel* lbl1=new QLabel("USB BOOT");
lbl1->setFont(font);
lbl1->setScaledContents(true);
lbl1->setStyleSheet("QLabel { color : blue; }");

vl->addWidget(lbl1,4,Qt::AlignHCenter);

// вложенный lm для файлселекторов
QGridLayout* gvl=new QGridLayout(0);
vl->addLayout(gvl);

QLabel* lbl2=new QLabel("usbloader:");
gvl->addWidget(lbl2,0,0);

qd->fname=new QLineEdit(qd);
if (strlen(filename) != 0) qd->fname->setText(filename);
gvl->addWidget(qd->fname,0,1);

QToolButton* fselector = new QToolButton(qd);
// fselector->setText("...");
fselector->setIcon(QIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon))); 
gvl->addWidget(fselector,0,2);

QLabel* lbl3=new QLabel("Partition table:");
gvl->addWidget(lbl3,1,0);

qd->ptfname=new QLineEdit(qd);
if (strlen(ptfilename) != 0) qd->ptfname->setText(ptfilename);
gvl->addWidget(qd->ptfname,1,1);

QToolButton* ptselector = new QToolButton(qd);
// ptselector->setText("...");
ptselector->setIcon(QIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon))); 
gvl->addWidget(ptselector,1,2);

QToolButton* ptclear = new QToolButton(qd);
// ptclear->setText("X");
ptclear->setIcon(QIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon))); 
gvl->addWidget(ptclear,1,3);

// кнопки выбора режима загрузки
QCheckBox* fbflag = new QCheckBox("Loading in FASTBOOT", qd);
vl->addWidget(fbflag);

QCheckBox* isbadflag= new QCheckBox("Disable control of bad blocks ", qd);
vl->addWidget(isbadflag);

QCheckBox* patchflag= new QCheckBox("Disable patch eraseall (DANGER !!!) ", qd);
vl->addWidget(patchflag);

QDialogButtonBox* buttonBox = new QDialogButtonBox(qd);
buttonBox->setOrientation(Qt::Horizontal);
buttonBox->addButton("Cancel",QDialogButtonBox::RejectRole);
buttonBox->addButton("Loading",QDialogButtonBox::AcceptRole);
vl->addWidget(buttonBox,10,Qt::AlignHCenter);

QObject::connect(buttonBox, SIGNAL(accepted()), qd, SLOT(accept()));
QObject::connect(buttonBox, SIGNAL(rejected()), qd, SLOT(reject()));
QObject::connect(fselector, SIGNAL(clicked()), qd, SLOT(browse()));
QObject::connect(ptselector, SIGNAL(clicked()), qd, SLOT(ptbrowse()));
QObject::connect(ptclear, SIGNAL(clicked()), qd, SLOT(ptclear()));

// Запускаем диалог
res=qd->exec();

// вынимаем данные из диалога
fflag=fbflag->isChecked();
pflag=patchflag->isChecked();
bflag=isbadflag->isChecked();
strcpy(filename,qd->fname->displayText().toLocal8Bit());
strcpy(ptfilename,qd->ptfname->displayText().toLocal8Bit());

// удаляем панель диалога
delete qd;

if (res != QDialog::Accepted) return;

//--------- Чтение загрузчика в память ---------------

// открываем файл загрузчика
in=fopen(filename,"r");
if (in == 0) {
  QMessageBox::critical(0, "Error", "Error open file");
  return;
}  
  

// Прверяем сигнатуру usloader
fread(&i,1,4,in);
if (i != 0x20000) {
  QMessageBox::critical(0, "Error", "The file is not a usbloader boot loader");
  fclose(in);
  return;
}  

// читаем заголовок загрузчика
fseek(in,36,SEEK_SET); // начало каталога компонентов в файле

fread(&part,sizeof(part),1,in);

// Ищем конец каталога компонентов
for (i=0;i<5;i++) {
  if (part[i].lmode == 0) break;
}
numparts=i;

// Загружаем компоненты в память
for(i=0;i<numparts;i++) {
 // встаем на начало образа компонента
 fseek(in,part[i].offset,SEEK_SET);
 // освобождаем предыдущий распределенный буфер
 if (pbuf[i] != 0) {
   free(pbuf[i]);
   pbuf[i]=0;
 }  
 // читаем в буфер весь компонент
 pbuf[i]=(uint8_t*)malloc(part[i].size);
 fsize=fread(pbuf[i],1,part[i].size,in);
 if (part[i].size != fsize) {
      QMessageBox::critical(0, "Error", "Unexpected end of file");
      fclose(in);
      return;
 }
 // общий размер загрузчика
 totalsize+=part[i].size;
}

fclose(in);

// делаем fastboot-патч
if (fflag) {
  if (!fastboot_only(pbuf,part)) return;
}  

// ERASE-патч
if (!pflag) {
  res=pv7r2(pbuf[1], part[1].size)+ pv7r11(pbuf[1], part[1].size) + pv7r1(pbuf[1], part[1].size) + pv7r22(pbuf[1], part[1].size) + pv7r22_2(pbuf[1], part[1].size);
  if (res == 0)  {
   QMessageBox::critical(0, "Error", "Patch signature not found, loading not done");
   return;
  }  
}  

// isbad-патч
if (bflag) {
  res=perasebad(pbuf[1], part[1].size);
  if (res == 0)  {
   QMessageBox::critical(0, "Error", "BAD ERASE signature not found, loading is not performed");
   return;
  }  
}  


// Замещаем таблицу разделов
if (strlen(ptfilename) != 0) {
 if (!ptable_patch(ptfilename, pbuf, part)) return;
} 

//-------------------------------------------------------------------  
// Настройка SIO
if (!open_port())  {
  QMessageBox::critical(0, "Error", "The serial port does not open");
  return;
}  


// Проверяем загрузочный порт
c=0;
write(siofd,"A",1);   // отправляем произвольный байт в порт
bl=read(siofd,&c,1);
// ответ должен быть U (0x55)
if (c != 0x55) {
  QMessageBox::critical(0, "Error", "The serial port is not in USB boot mode");
  close_port();
  return;
}  


// Формируем панель индикаторов
QDialog* ind=new QDialog;
QFormLayout* lmf=new QFormLayout(ind);

QProgressBar* partbar = new QProgressBar(ind);
partbar->setValue(0);
lmf->addRow("Section:",partbar);

QProgressBar* totalbar = new QProgressBar(ind);
totalbar->setValue(0);
lmf->addRow("Total:",totalbar);

ind->show();

// главный цикл загрузки - загружаем все блоки, найденные в заголовке

for(bl=0;bl<numparts;bl++) {

  
 // стартовый пакет  
 if (!start_part(part[bl].size,part[bl].adr,part[bl].lmode)) {
   QMessageBox::critical(0, "Error", "Component header rejected by the modem");
   goto leave;
 }  

  // Цикл поблочной загрузки данных
  datasize=1024;
  pktcount=1;
  for(adr=0;adr<part[bl].size;adr+=1024) {
    // проверка на последний блок компонента
    if ((adr+1024)>=part[bl].size) datasize=part[bl].size-adr; 
     
    // обновляем прогрессбар блоков 
    partbar->setValue(adr*100/part[bl].size);            // для раздела
    totalbar->setValue((loadedsize+adr)*100/totalsize);  // общий
    QCoreApplication::processEvents();
    
    if (!send_data_packet(pktcount++,(uint8_t*)(pbuf[bl]+adr),datasize)) {
      QMessageBox::critical(0, "Error", "Data packet rejected by modem");
      goto leave;
    }  
  }
  // обновляем размер уже загруженных данных
  loadedsize+=part[bl].size;


  if (!close_part(pktcount)) {
      QMessageBox::critical(0,"Erreur","Le modem rejette la commande pour terminer le composant");
      goto leave;
    }  
} 

totalbar->setValue(100);
partbar->setValue(100);
QCoreApplication::processEvents();
      
QMessageBox::information(0,"ОК", "Download complete");

leave:
close_port();
delete ind;

}

  
