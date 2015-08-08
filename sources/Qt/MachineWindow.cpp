/*	_________  ___
  _____ \_   /\  \/  /	Qt/MachineWindow.cpp
 |  |  |_/  /__>    <	Copyright © 2014-2015 Manuel Sainz de Baranda y Goñi.
 |   ____________/\__\	Released under the terms of the GNU General Public License v3.
 |_*/

#include "MachineWindow.hpp"
#include "ui_MachineWindow.h"
#include "AboutDialog.hpp"

#include <QFile>
#include <QMessageBox>
#include <QKeyEvent>
#include <QFileDialog>
#include <math.h>

namespace Q {
#	include <Q/keys/layout.h>
#	include <Q/functions/buffering/QTripleBuffer.h>
#	include <Q/functions/buffering/QRingBuffer.h>
#	include <Q/functions/geometry/QRectangle.h>
#	include <Q/hardware/machine/model/computer/ZX Spectrum/ZX Spectrum.h>
#	include "system.h"
#	include <Q/types/time.h>
}

#define MACHINE_SCREEN_SIZE Q::q_2d((Q::qreal)Q_ZX_SPECTRUM_SCREEN_WIDTH, (Q::qreal)Q_ZX_SPECTRUM_SCREEN_HEIGHT)


static AboutDialog* aboutDialog = NULL;


static void *emulate_machine_entry(MachineWindow *machine_window)
	{
	machine_window->emulateMachine();
	return NULL;
	}


void MachineWindow::emulateMachine()
	{
	Q::quint64  frames_per_second = 50;
	Q::quint64  frame_ticks	      = 1000000000 / frames_per_second;
	Q::quint64  next_frame_tick   = Q::q_ticks();
	Q::quint64  delta;
	Q::quint    maximum_frameskip = 5;
	Q::quint    loops;
	//void*	    audio_output_buffer;
	Q::quint64* keyboard;

	while (!mustStop)
		{
		loops = 0;

		do abi->run_one_frame(machine);
		while ((next_frame_tick += frame_ticks) < Q::q_ticks() && ++loops < maximum_frameskip);

//		if ((audio_output_buffer = Q::q_ring_buffer_try_produce(this->audio_output_buffer)) != NULL)
//			machine->audio_output_buffer = (Q::qint16 *)audio_output_buffer;

		machine->video_output_buffer = Q::q_triple_buffer_produce(videoOutputBuffer);
		//qDebug("thread new output buffer -> %p", machine->video_output_buffer);

		/*---------------.
		| Consume input. |
		'---------------*/
		if ((keyboard = (Q::quint64 *)Q::q_triple_buffer_consume(keyboardBuffer)) != NULL)
			{machine->state.keyboard.uint64_value = *keyboard;}

		/*---------------------------------------.
		| Schedule next iteration time and wait. |
		'---------------------------------------*/
		if ((delta = next_frame_tick - Q::q_ticks()) <= frame_ticks)
			Q::q_wait(delta);

		//qDebug("delta => %lu, next => %lu", delta, next_frame_tick);
		}
	}


void MachineWindow::runMachine()
	{
	flags.running = true;
	mustStop = false;
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_create(&thread, &attributes, (void *(*)(void *))emulate_machine_entry, this);
	pthread_attr_destroy(&attributes);
	}


void MachineWindow::stopMachine()
	{
	flags.running = false;
	mustStop = true;
	pthread_join(thread, NULL);
	//abi->reset(machine);
	}
/*
QFrame frame;
	QPalette palette;

	palette.setColor(QPalette::Background,Qt::red);
	frame.setFixedSize(240,240);
	frame.setPalette(palette);
	frame.setWindowTitle("QFrame Background Color");
	frame.show();
 **/


double MachineWindow::currentZoom()
	{
	return isFullScreen()
		? ui->videoOutputView->contentSize().x / double(Q_ZX_SPECTRUM_SCREEN_WIDTH)
		: double(ui->videoOutputView->width()) / double(Q_ZX_SPECTRUM_SCREEN_WIDTH);
	}


void MachineWindow::setZoom(double zoom)
	{
	if (isFullScreen())
		{
		Q::Q2D boundsSize = Q::q_2d(ui->videoOutputView->width(), ui->videoOutputView->height());
		Q::Q2D zoomedSize = Q::q_2d(double(Q_ZX_SPECTRUM_SCREEN_WIDTH) * zoom, double(Q_ZX_SPECTRUM_SCREEN_HEIGHT) * zoom);

		ui->videoOutputView->setContentSize(Q::q_2d_contains(boundsSize, zoomedSize)
			? zoomedSize
			: q_2d_fit(MACHINE_SCREEN_SIZE, boundsSize));
		}

	else	{
		}
	}


MachineWindow::MachineWindow(QWidget *parent) :	QMainWindow(parent), ui(new Ui::MachineWindow)
	{
	ui->setupUi(this);
	addActions(ui->menuBar->actions());
	ui->videoOutputView->setFocus(Qt::OtherFocusReason);

	qDebug("%d", hasMouseTracking());
	//ui->centralwidget->layout()->setMenuBar(ui->menubar);
	//setMenuBar(NULL);

	flags.running = false;

	//---------------------------------------.
	// Create the machine and its components |
	//---------------------------------------'
	abi = &Q::machine_abi_table[4];

	machine		       = (Q::ZXSpectrum *)malloc(abi->context_size);
	machine->cpu_abi.run   = (Q::ACMERunCycles)Q::z80_run;
	machine->cpu_abi.irq   = (Q::ACMESwitch)Q::z80_irq;
	machine->cpu_abi.reset = (Q::ACMEPulse)Q::z80_reset;
	machine->cpu_abi.power = (Q::ACMESwitch)Q::z80_power;
	machine->cpu	       = (Q::Z80 *)malloc(sizeof(Q::Z80));
	machine->cpu_cycles    = &machine->cpu->ticks;
	machine->memory	       = (Q::quint8 *)malloc(abi->memory_size);

	memset(machine->memory, 0, abi->memory_size);

	ui->videoOutputView->setResolutionAndFormat
		(Q::q_2d_value(SIZE)(Q_ZX_SPECTRUM_SCREEN_WIDTH, Q_ZX_SPECTRUM_SCREEN_HEIGHT), 0);

	videoOutputBuffer = ui->videoOutputView->getBuffer();
	machine->video_output_buffer = Q::q_triple_buffer_production_buffer(videoOutputBuffer);
	qDebug("video buffer -> %p", machine->video_output_buffer);
	machine->audio_output_buffer = (Q::qint16 *)malloc(sizeof(qint16) * 882 * 4);
	//machine->audio_output_buffer = (Q::qint16 *)Q::q_ring_buffer_production_buffer(audio_output_buffer);

	keyboardBuffer = (Q::QTripleBuffer *)malloc(sizeof(Q::QTripleBuffer));
	Q::q_triple_buffer_initialize(keyboardBuffer, malloc(sizeof(Q::quint64) * 3), sizeof(Q::quint64));
	keyboard = (Q::Q64Bit *)q_triple_buffer_production_buffer(keyboardBuffer);
	memset(keyboardBuffer->buffers[0], 0xFF, sizeof(Q::quint64) * 3);

	Q::qsize index = abi->rom_count;
	Q::ROM *rom;

	while (index)
		{
		rom = &abi->roms[--index];

		QString fileName;
		fileName.sprintf("/home/ciro/ROMs/NEStalin/%s.rom", rom->file_name);

		QFile file(fileName);

		if (!file.open(QIODevice::ReadOnly))
			{
			QMessageBox::information(this, tr("Unable to open file"),
			file.errorString());
			}

		else	{
			file.read((char *)(machine->memory + rom->base_address), rom->size);
			file.close();
			}
		}

	abi->initialize(machine);
	keyboardState.uint64_value = Q_UINT64(0xFFFFFFFFFFFFFFFF);
	}


MachineWindow::~MachineWindow()
	{
	if (flags.running) stopMachine();
	delete ui;
	}


#define KEY_DOWN(line, bit) keyboardState.uint8_array[line] &= ~(1 << bit)
#define KEY_UP(  line, bit) keyboardState.uint8_array[line] |=  (1 << bit)


void MachineWindow::keyPressEvent(QKeyEvent* event)
	{
	//int key = event->key();
	//qDebug("%c => %d\n", (char)key, key);
	//Qt::Key
	switch (event->key())
		{
		case Qt::Key_Escape:
		if (isFullScreen())
			{
			if (fullScreenMenuFrame->isVisible())
				{
				fullScreenMenuFrame->hide();
				QApplication::setOverrideCursor(Qt::BlankCursor);
				}

			else	{
				fullScreenMenuFrame->show();
				QApplication::restoreOverrideCursor();
				}
			}
		break;

		// Line 0
		case Qt::Key_Shift: KEY_DOWN(0, 0); break;
		case Qt::Key_Z: KEY_DOWN(0, 1); break;
		case Qt::Key_X: KEY_DOWN(0, 2); break;
		case Qt::Key_C: KEY_DOWN(0, 3); break;
		case Qt::Key_V: KEY_DOWN(0, 4); break;

		// Line 1
		case Qt::Key_A: KEY_DOWN(1, 0); break;
		case Qt::Key_S: KEY_DOWN(1, 1); break;
		case Qt::Key_D: KEY_DOWN(1, 2); break;
		case Qt::Key_F: KEY_DOWN(1, 3); break;
		case Qt::Key_G: KEY_DOWN(1, 4); break;

		//Line 2
		case Qt::Key_Q: KEY_DOWN(2, 0); break;
		case Qt::Key_W: KEY_DOWN(2, 1); break;
		case Qt::Key_E: KEY_DOWN(2, 2); break;
		case Qt::Key_R: KEY_DOWN(2, 3); break;
		case Qt::Key_T: KEY_DOWN(2, 4); break;

		// Line 3
		case Qt::Key_1: KEY_DOWN(3, 0); break;
		case Qt::Key_2: KEY_DOWN(3, 1); break;
		case Qt::Key_3: KEY_DOWN(3, 2); break;
		case Qt::Key_4: KEY_DOWN(3, 3); break;
		case Qt::Key_5: KEY_DOWN(3, 4); break;

		// Line 4
		case Qt::Key_0: KEY_DOWN(4, 0); break;
		case Qt::Key_9: KEY_DOWN(4, 1); break;
		case Qt::Key_8: KEY_DOWN(4, 2); break;
		case Qt::Key_7: KEY_DOWN(4, 3); break;
		case Qt::Key_6: KEY_DOWN(4, 4); break;

		// Line 5
		case Qt::Key_P: KEY_DOWN(5, 0); break;
		case Qt::Key_O: KEY_DOWN(5, 1); break;
		case Qt::Key_I: KEY_DOWN(5, 2); break;
		case Qt::Key_U: KEY_DOWN(5, 3); break;
		case Qt::Key_Y: KEY_DOWN(5, 4); break;

		// Line 6
		case Qt::Key_Return: KEY_DOWN(6, 0); break;
		case Qt::Key_L: KEY_DOWN(6, 1); break;
		case Qt::Key_K: KEY_DOWN(6, 2); break;
		case Qt::Key_J: KEY_DOWN(6, 3); break;
		case Qt::Key_H: KEY_DOWN(6, 4); break;

		// Line 7
		case Qt::Key_Space: KEY_DOWN(7, 0); break;
		case Qt::Key_Alt: KEY_DOWN(7, 1); break;
		case Qt::Key_M: KEY_DOWN(7, 2); break;
		case Qt::Key_N: KEY_DOWN(7, 3); break;
		case Qt::Key_B: KEY_DOWN(7, 4); break;

		// Composed
		case Qt::Key_Backspace:	KEY_DOWN(4, 0); KEY_DOWN(0, 0); break;
		case Qt::Key_Comma:	KEY_DOWN(7, 1); KEY_DOWN(7, 3); break;
		case Qt::Key_Down:	KEY_DOWN(0, 0); KEY_DOWN(4, 4); break;
		case Qt::Key_Up:	KEY_DOWN(0, 0); KEY_DOWN(4, 3); break;
		case Qt::Key_Left:	KEY_DOWN(0, 0); KEY_DOWN(3, 4); break;
		case Qt::Key_Right:	KEY_DOWN(0, 0); KEY_DOWN(4, 2); break;

		default: break;
		}

	keyboard->uint64_value = keyboardState.uint64_value;
	keyboard = (Q::Q64Bit *)Q::q_triple_buffer_produce(keyboardBuffer);
	}


void MachineWindow::keyReleaseEvent(QKeyEvent* event)
	{
	switch (event->key())
		{
		// Line 0
		case Qt::Key_Shift: KEY_UP(0, 0); break;
		case Qt::Key_Z: KEY_UP(0, 1); break;
		case Qt::Key_X: KEY_UP(0, 2); break;
		case Qt::Key_C: KEY_UP(0, 3); break;
		case Qt::Key_V: KEY_UP(0, 4); break;
		// Line 1
		case Qt::Key_A: KEY_UP(1, 0); break;
		case Qt::Key_S: KEY_UP(1, 1); break;
		case Qt::Key_D: KEY_UP(1, 2); break;
		case Qt::Key_F: KEY_UP(1, 3); break;
		case Qt::Key_G: KEY_UP(1, 4); break;
		//Line 2
		case Qt::Key_Q: KEY_UP(2, 0); break;
		case Qt::Key_W: KEY_UP(2, 1); break;
		case Qt::Key_E: KEY_UP(2, 2); break;
		case Qt::Key_R: KEY_UP(2, 3); break;
		case Qt::Key_T: KEY_UP(2, 4); break;
		// Line 3
		case Qt::Key_1: KEY_UP(3, 0); break;
		case Qt::Key_2: KEY_UP(3, 1); break;
		case Qt::Key_3: KEY_UP(3, 2); break;
		case Qt::Key_4: KEY_UP(3, 3); break;
		case Qt::Key_5: KEY_UP(3, 4); break;
		// Line 4
		case Qt::Key_0: KEY_UP(4, 0); break;
		case Qt::Key_9: KEY_UP(4, 1); break;
		case Qt::Key_8: KEY_UP(4, 2); break;
		case Qt::Key_7: KEY_UP(4, 3); break;
		case Qt::Key_6: KEY_UP(4, 4); break;
		// Line 5
		case Qt::Key_P: KEY_UP(5, 0); break;
		case Qt::Key_O: KEY_UP(5, 1); break;
		case Qt::Key_I: KEY_UP(5, 2); break;
		case Qt::Key_U: KEY_UP(5, 3); break;
		case Qt::Key_Y: KEY_UP(5, 4); break;
		// Line 6
		case Qt::Key_Return: KEY_UP(6, 0); break;
		case Qt::Key_L: KEY_UP(6, 1); break;
		case Qt::Key_K: KEY_UP(6, 2); break;
		case Qt::Key_J: KEY_UP(6, 3); break;
		case Qt::Key_H: KEY_UP(6, 4); break;
		// Line 7
		case Qt::Key_Space:	 KEY_UP(7, 0); break;
		case Qt::Key_Alt: KEY_UP(7, 1); break;
		case Qt::Key_M: KEY_UP(7, 2); break;
		case Qt::Key_N: KEY_UP(7, 3); break;
		case Qt::Key_B: KEY_UP(7, 4); break;
		// Composed
		case Qt::Key_Backspace:	KEY_UP(4, 0); KEY_UP(0, 0); break;
		case Qt::Key_Comma:	KEY_UP(7, 1); KEY_UP(7, 3); break;
		case Qt::Key_Down:	KEY_UP(0, 0); KEY_UP(4, 4); break;
		case Qt::Key_Up:	KEY_UP(0, 0); KEY_UP(4, 3); break;
		case Qt::Key_Left:	KEY_UP(0, 0); KEY_UP(3, 4); break;
		case Qt::Key_Right:	KEY_UP(0, 0); KEY_UP(4, 2); break;

		default: break;
		}

	keyboard->uint64_value = keyboardState.uint64_value;
	keyboard = (Q::Q64Bit *)Q::q_triple_buffer_produce(keyboardBuffer);
	}


void MachineWindow::mouseDoubleClickEvent(QMouseEvent *)
	{
	bool enabled = !ui->actionViewFullScreen->isChecked();

	ui->actionViewFullScreen->setChecked(enabled);
	}


void MachineWindow::resizeEvent(QResizeEvent *)
	{
	if (isFullScreen())
		{
		ui->videoOutputView->setScaling(Q_SCALING_NONE);

		ui->videoOutputView->setContentSize(Q::q_2d_fit
			(Q::q_2d(Q_ZX_SPECTRUM_SCREEN_WIDTH, Q_ZX_SPECTRUM_SCREEN_HEIGHT),
			 Q::q_2d(ui->videoOutputView->width(), ui->videoOutputView->height())));

		fullScreenMenuFrame->setGeometry(0.0, 0.0, ui->videoOutputView->width(), ui->menuBar->height());
		fullScreenMenuFrame->show();
		}
	}


void MachineWindow::aboutDialogClosed()
	{aboutDialog = NULL;}


void MachineWindow::on_actionFileNewWindow_triggered()
	{
	}


void MachineWindow::on_actionFileOpen_triggered()
	{
	QString fileName = QFileDialog::getOpenFileName
		(this, tr("Open ROM Image"), "", tr("NES ROM Images (*.nes)"));
	}



void MachineWindow::on_actionFileQuit_triggered()
	{
	exit(0);
	}


void MachineWindow::on_actionMachinePower_toggled(bool enabled)
	{
	ui->actionMachinePause->setChecked(false);
	ui->actionMachinePause->setEnabled(enabled);
	ui->actionMachineReset->setEnabled(enabled);

	if (enabled)
		{
		abi->power(machine, ON);
		runMachine();
		ui->videoOutputView->start();
		}

	else	{
		ui->videoOutputView->stop();
		stopMachine();
		abi->power(machine, OFF);
		}
	}


void MachineWindow::on_actionMachinePause_toggled(bool enabled)
	{
	if (enabled) stopMachine();
	else runMachine();
	}


void MachineWindow::on_actionMachineReset_triggered()
	{
	ui->actionMachinePause->setChecked(false);
	if (flags.running) stopMachine();
	abi->reset(machine);
	runMachine();
	}


void MachineWindow::on_actionViewFullScreen_toggled(bool enabled)
	{
	if (enabled)
		{
		qDebug("FULL");
		showFullScreen();
		fullScreenMenuFrame = new QFrame(this);
		fullScreenMenuFrame->setAutoFillBackground(true);
		QHBoxLayout layout;
		fullScreenMenuFrame->setLayout(&layout);
		layout.setMenuBar(ui->menuBar);
		setMenuBar(NULL);
		}

	else	{
		//setMenuBar(ui->menubar);
		ui->menuBar->show();
		ui->videoOutputView->setScaling(Q_SCALING_FIT);
		if (fullScreenMenuFrame->isHidden()) QApplication::restoreOverrideCursor();
		setMenuBar(ui->menuBar);
		//fullScreenMenuFrame->layout()->setMenuBar(NULL);
		fullScreenMenuFrame->setParent(NULL);
		delete fullScreenMenuFrame;
		fullScreenMenuFrame = NULL;
		showNormal();
		}
	}


void MachineWindow::on_actionViewZoomIn_triggered()
	{setZoom(floor(currentZoom() / 0.5) * 0.5 + 0.5);}


void MachineWindow::on_actionViewZoomOut_triggered()
	{
	double factor = floor(currentZoom() / 0.5) * 0.5 - 0.5;

	setZoom(factor <= 1.0 ? 1.0 : factor);
	}


void MachineWindow::on_actionView1x_triggered() {setZoom(1.0);}
void MachineWindow::on_actionView2x_triggered() {setZoom(2.0);}
void MachineWindow::on_actionView3x_triggered() {setZoom(3.0);}


void MachineWindow::on_actionHelpAbout_triggered()
	{
	if (!aboutDialog)
		{
		(aboutDialog = new AboutDialog(this))->setAttribute(Qt::WA_DeleteOnClose);
		connect(aboutDialog, SIGNAL(finished(int)), this, SLOT(aboutDialogClosed()));
		}

	aboutDialog->show();
	aboutDialog->raise();
	}


// Qt/MachineWindow.cpp EOF