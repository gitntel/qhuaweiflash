// просмотр и редактирование произвольных файлов 

#include "viewer.h"

//***********************************************************
//* Конструктор просмотрщика
//***********************************************************
viewer::viewer(uint8_t* srcdata, uint32_t* srclen, uint8_t rmode, char* fname, cpfiledir* dfile) : QMainWindow() {
  
QString title;
QFont font;
uint32_t plen;

// настройки геометрии окна
show();  
setAttribute(Qt::WA_DeleteOnClose);

config=new QSettings("forth32","qhuaweiflash",this);
QRect rect=config->value("/config/EditorRect").toRect();
if (rect != QRect(0,0,0,0)) setGeometry(rect);
// выводим окно на передний план
setFocus();
raise();
activateWindow();

// сохраняем на будущее входные параметры  
fileptr=dfile;
readonly=rmode;
sdata=srcdata;
slen=srclen;

// определяем размер буфера и создаем его
if (fileptr != 0)  plen=fileptr->fsize();
else plen=*slen;
pdata=new uint8_t[plen+1];

// копируем данные в локальный буфер
if (fileptr != 0) memcpy(pdata,fileptr->fdata(),plen);
else memcpy(pdata,srcdata,plen);

// ограничитель строки
pdata[plen]=0; 

// заголовок окна
if (readonly) title="View -";
else title="Mounting -";
title.append(fname);
setWindowTitle(title);

// Главное меню
menubar = new QMenuBar(this);
setMenuBar(menubar);

menu_file = new QMenu("File",menubar);
menubar->addAction(menu_file->menuAction());

menu_edit = new QMenu("Edit",menubar);
menubar->addAction(menu_edit->menuAction());

menu_view = new QMenu("Exit",menubar);
menubar->addAction(menu_view->menuAction());


// тулбар
toolbar=new QToolBar(this);
addToolBar(toolbar);

// Центральный виджет
central=new QWidget(this);
setCentralWidget(central);

// основной компоновщик
vlm=new QVBoxLayout(central);

// текстовый редактор
ted=new QTextEdit(central);
ted->setReadOnly(readonly);
vlm->addWidget(ted,2);

// шрифт редактора
font=qvariant_cast<QFont>(config->value("/config/EditorFont"));
ted->setFont(font);

// наполнение текстового редактора
textdata=(char*)pdata;
ted->append(textdata);

// пункты меню
menu_file->addAction(QIcon::fromTheme("document-save"),"Save",this,SLOT(save_all()),QKeySequence::Save);
toolbar->addAction(QIcon::fromTheme("document-save"),"Save",this,SLOT(save_all()));
menu_file->addSeparator();
menu_file->addAction("Exit",this,SLOT(close()),QKeySequence("Esc"));

toolbar->addSeparator();

if (!readonly) {
  menu_edit->addAction(QIcon::fromTheme("edit-undo"),"Undo",ted,SLOT(undo()),QKeySequence::Undo);
  toolbar->addAction(QIcon::fromTheme("edit-undo"),"Undo",ted,SLOT(undo()));
  menu_edit->addAction(QIcon::fromTheme("edit-redo"),"Redo",ted,SLOT(redo()),QKeySequence::Redo);
  toolbar->addAction(QIcon::fromTheme("edit-redo"),"Redo",ted,SLOT(redo()));
  menu_edit->addSeparator();
  menu_edit->addAction(QIcon::fromTheme("edit-cut"),"Cut",ted,SLOT(cut()),QKeySequence::Cut);
  toolbar->addAction(QIcon::fromTheme("edit-cut"),"Cut",ted,SLOT(cut()));
}
menu_edit->addAction(QIcon::fromTheme("edit-copy"),"Copy",ted,SLOT(copy()),QKeySequence::Copy);
toolbar->addAction(QIcon::fromTheme("edit-copy"),"Copy",ted,SLOT(copy()));

if (!readonly) {
  menu_edit->addAction(QIcon::fromTheme("edit-paste"),"Paste",ted,SLOT(paste()),QKeySequence::Paste);
  toolbar->addAction(QIcon::fromTheme("edit-paste"),"Paste",ted,SLOT(paste()));
  toolbar->addSeparator();
}
menu_edit->addAction(QIcon::fromTheme("edit-find"),"Find ...",this,SLOT(find()),QKeySequence::Find);
toolbar->addAction(QIcon::fromTheme("edit-find"),"Find ...",this,SLOT(find()));
menu_edit->addAction(QIcon::fromTheme("edit-find"),"Find further",this,SLOT(findnext()),QKeySequence::FindNext);


menu_view->addAction(QIcon::fromTheme("zoom-in"),"Zoom in",ted,SLOT(zoomIn()),QKeySequence("Ctrl++"));
toolbar->addAction(QIcon::fromTheme("zoom-in"),"Zoom in",ted,SLOT(zoomIn()));
menu_view->addAction(QIcon::fromTheme("zoom-out"),"Zoom out",ted,SLOT(zoomOut()),QKeySequence("Ctrl+-"));
toolbar->addAction(QIcon::fromTheme("zoom-out"),"Zoom out",ted,SLOT(zoomOut()));
menu_view->addAction(QIcon::fromTheme("preferences-desktop-font"),"Font ...",this,SLOT(fontselector()));

// слот модификации
connect(ted,SIGNAL(textChanged()),this,SLOT(setChanged()));

ted->setFocus();
ted->moveCursor(QTextCursor::Start,QTextCursor::MoveAnchor);
}

//***********************************************************
//* Деструктор просмотрщика
//***********************************************************
viewer::~viewer() {

QMessageBox::StandardButton reply;
QFont font;

// сохраняем размер шрифта
font=ted->font();
config->setValue("/config/EditorFont",font);

// геометрия главного окна
QRect rect=geometry();
config->setValue("/config/EditorRect",rect);

// признак изменения данных
if (datachanged) {
  reply=QMessageBox::warning(this, "Write the file", "The content file changed, save?",QMessageBox::Ok | QMessageBox::Cancel);
  if (reply == QMessageBox::Ok) {
    // сохранение данных
    save_all();
  }
}  
  
delete config;
delete [] pdata;  
}

//***********************************************************
//* Сохранение данных в вектор файла
//***********************************************************
void viewer::save_all() {

QByteArray xdata;
QString str;
int pos;

textdata=ted->toPlainText();
xdata=textdata.toLocal8Bit();
if (fileptr != 0) fileptr->replace_data((uint8_t*)xdata.data(),xdata.size());
else {
  delete [] sdata;
  sdata=new uint8_t[xdata.size()];
  memcpy(sdata,(uint8_t*)xdata.data(),xdata.size());
  *slen=xdata.size();
}  
// вызываем сигнал- признак модификации
emit changed();

// удаляем звездочку из заголовка
str=windowTitle();
pos=str.indexOf('*');
if (pos != -1) {
  str.truncate(pos-1);
  setWindowTitle(str);
}  
// восстанавливаем обработчик модификации
datachanged=false;
connect(ted,SIGNAL(textChanged()),this,SLOT(setChanged()));

}

//***********************************************************
//* Поиск текста
//***********************************************************
void viewer::find() {

int res;  
  
QInputDialog* pd=new QInputDialog(this);  
res=pd->exec();
if (res == QDialog::Accepted) {
 findtext=pd->textValue();
 findnext();
}
delete pd;
}

//***********************************************************
//* Продолжение поиска текста
//***********************************************************
void viewer::findnext() {

int res;

if (findtext.size() == 0) return;
  res=ted->find(findtext);
  if (!res) {
    QMessageBox::information(0,"Information ", " Text not found ");
  } 
}  

//***********************************************************
//* Выбор шрифта
//***********************************************************
void viewer::fontselector() {

int res;

QFont font=ted->font();
QFontDialog* fss=new QFontDialog(this);
fss->setCurrentFont(font);
res=fss->exec();
if (res == QDialog::Accepted) {
  font=fss->selectedFont();
  ted->setFont(font);
}
delete fss;
}


//***********************************************************
//* Вызов внешнего слота модификации
//***********************************************************
void viewer::setChanged() { 

QString str;

datachanged=true;
// рассоединяем сигнал - он нужен ровно один раз
disconnect(ted,SIGNAL(textChanged()),this,SLOT(setChanged()));
// добавляем звездочку в заголовок
str=windowTitle();
str.append(" *");
setWindowTitle(str);
}
