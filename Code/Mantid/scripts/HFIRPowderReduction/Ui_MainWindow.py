# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'Ui_MainWindow.ui'
#
# Created: Mon Mar  9 15:26:13 2015
#      by: PyQt4 UI code generator 4.11.2
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

try:
    _fromUtf8 = QtCore.QString.fromUtf8
except AttributeError:
    def _fromUtf8(s):
        return s

try:
    _encoding = QtGui.QApplication.UnicodeUTF8
    def _translate(context, text, disambig):
        return QtGui.QApplication.translate(context, text, disambig, _encoding)
except AttributeError:
    def _translate(context, text, disambig):
        return QtGui.QApplication.translate(context, text, disambig)

class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName(_fromUtf8("MainWindow"))
        MainWindow.resize(1200, 1024)
        self.centralwidget = QtGui.QWidget(MainWindow)
        self.centralwidget.setObjectName(_fromUtf8("centralwidget"))
        self.gridLayout = QtGui.QGridLayout(self.centralwidget)
        self.gridLayout.setObjectName(_fromUtf8("gridLayout"))
        self.verticalLayout = QtGui.QVBoxLayout()
        self.verticalLayout.setObjectName(_fromUtf8("verticalLayout"))
        self.horizontalLayout = QtGui.QHBoxLayout()
        self.horizontalLayout.setObjectName(_fromUtf8("horizontalLayout"))
        self.label_expNo = QtGui.QLabel(self.centralwidget)
        self.label_expNo.setObjectName(_fromUtf8("label_expNo"))
        self.horizontalLayout.addWidget(self.label_expNo)
        self.lineEdit_expNo = QtGui.QLineEdit(self.centralwidget)
        self.lineEdit_expNo.setObjectName(_fromUtf8("lineEdit_expNo"))
        self.horizontalLayout.addWidget(self.lineEdit_expNo)
        self.label_scanNo = QtGui.QLabel(self.centralwidget)
        self.label_scanNo.setObjectName(_fromUtf8("label_scanNo"))
        self.horizontalLayout.addWidget(self.label_scanNo)
        self.lineEdit_scanNo = QtGui.QLineEdit(self.centralwidget)
        self.lineEdit_scanNo.setObjectName(_fromUtf8("lineEdit_scanNo"))
        self.horizontalLayout.addWidget(self.lineEdit_scanNo)
        self.pushButton_loadData = QtGui.QPushButton(self.centralwidget)
        self.pushButton_loadData.setObjectName(_fromUtf8("pushButton_loadData"))
        self.horizontalLayout.addWidget(self.pushButton_loadData)
        self.label_calibration = QtGui.QLabel(self.centralwidget)
        self.label_calibration.setObjectName(_fromUtf8("label_calibration"))
        self.horizontalLayout.addWidget(self.label_calibration)
        spacerItem = QtGui.QSpacerItem(40, 20, QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Minimum)
        self.horizontalLayout.addItem(spacerItem)
        self.verticalLayout.addLayout(self.horizontalLayout)
        self.horizontalLayout_2 = QtGui.QHBoxLayout()
        self.horizontalLayout_2.setObjectName(_fromUtf8("horizontalLayout_2"))
        self.tabWidget = QtGui.QTabWidget(self.centralwidget)
        self.tabWidget.setObjectName(_fromUtf8("tabWidget"))
        self.tab = QtGui.QWidget()
        self.tab.setObjectName(_fromUtf8("tab"))
        self.gridLayout_2 = QtGui.QGridLayout(self.tab)
        self.gridLayout_2.setObjectName(_fromUtf8("gridLayout_2"))
        self.verticalLayout_2 = QtGui.QVBoxLayout()
        self.verticalLayout_2.setObjectName(_fromUtf8("verticalLayout_2"))
        self.horizontalLayout_3 = QtGui.QHBoxLayout()
        self.horizontalLayout_3.setObjectName(_fromUtf8("horizontalLayout_3"))
        self.label_normalizeMonitor = QtGui.QLabel(self.tab)
        self.label_normalizeMonitor.setObjectName(_fromUtf8("label_normalizeMonitor"))
        self.horizontalLayout_3.addWidget(self.label_normalizeMonitor)
        self.lineEdit_normalizeMonitor = QtGui.QLineEdit(self.tab)
        self.lineEdit_normalizeMonitor.setObjectName(_fromUtf8("lineEdit_normalizeMonitor"))
        self.horizontalLayout_3.addWidget(self.lineEdit_normalizeMonitor)
        spacerItem1 = QtGui.QSpacerItem(40, 20, QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Minimum)
        self.horizontalLayout_3.addItem(spacerItem1)
        self.label_outputFormat = QtGui.QLabel(self.tab)
        self.label_outputFormat.setObjectName(_fromUtf8("label_outputFormat"))
        self.horizontalLayout_3.addWidget(self.label_outputFormat)
        self.comboBox_outputFormat = QtGui.QComboBox(self.tab)
        self.comboBox_outputFormat.setObjectName(_fromUtf8("comboBox_outputFormat"))
        self.horizontalLayout_3.addWidget(self.comboBox_outputFormat)
        self.lineEdit_outputFileName = QtGui.QLineEdit(self.tab)
        self.lineEdit_outputFileName.setObjectName(_fromUtf8("lineEdit_outputFileName"))
        self.horizontalLayout_3.addWidget(self.lineEdit_outputFileName)
        self.pushButton_saveData = QtGui.QPushButton(self.tab)
        self.pushButton_saveData.setObjectName(_fromUtf8("pushButton_saveData"))
        self.horizontalLayout_3.addWidget(self.pushButton_saveData)
        spacerItem2 = QtGui.QSpacerItem(40, 20, QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Minimum)
        self.horizontalLayout_3.addItem(spacerItem2)
        self.verticalLayout_2.addLayout(self.horizontalLayout_3)
        self.horizontalLayout_6 = QtGui.QHBoxLayout()
        self.horizontalLayout_6.setObjectName(_fromUtf8("horizontalLayout_6"))
        self.label_detExcluded = QtGui.QLabel(self.tab)
        self.label_detExcluded.setObjectName(_fromUtf8("label_detExcluded"))
        self.horizontalLayout_6.addWidget(self.label_detExcluded)
        self.lineEdit_detExcluded = QtGui.QLineEdit(self.tab)
        self.lineEdit_detExcluded.setObjectName(_fromUtf8("lineEdit_detExcluded"))
        self.horizontalLayout_6.addWidget(self.lineEdit_detExcluded)
        spacerItem3 = QtGui.QSpacerItem(40, 20, QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Minimum)
        self.horizontalLayout_6.addItem(spacerItem3)
        self.label_wavelength = QtGui.QLabel(self.tab)
        self.label_wavelength.setObjectName(_fromUtf8("label_wavelength"))
        self.horizontalLayout_6.addWidget(self.label_wavelength)
        self.lineEdit_wavelength = QtGui.QLineEdit(self.tab)
        self.lineEdit_wavelength.setObjectName(_fromUtf8("lineEdit_wavelength"))
        self.horizontalLayout_6.addWidget(self.lineEdit_wavelength)
        spacerItem4 = QtGui.QSpacerItem(40, 20, QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Minimum)
        self.horizontalLayout_6.addItem(spacerItem4)
        self.verticalLayout_2.addLayout(self.horizontalLayout_6)
        self.horizontalLayout_5 = QtGui.QHBoxLayout()
        self.horizontalLayout_5.setObjectName(_fromUtf8("horizontalLayout_5"))
        self.graphicsView_reducedData = QtGui.QGraphicsView(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Expanding)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.graphicsView_reducedData.sizePolicy().hasHeightForWidth())
        self.graphicsView_reducedData.setSizePolicy(sizePolicy)
        self.graphicsView_reducedData.setObjectName(_fromUtf8("graphicsView_reducedData"))
        self.horizontalLayout_5.addWidget(self.graphicsView_reducedData)
        self.verticalLayout_3 = QtGui.QVBoxLayout()
        self.verticalLayout_3.setObjectName(_fromUtf8("verticalLayout_3"))
        self.label_xmin = QtGui.QLabel(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Preferred, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.label_xmin.sizePolicy().hasHeightForWidth())
        self.label_xmin.setSizePolicy(sizePolicy)
        self.label_xmin.setObjectName(_fromUtf8("label_xmin"))
        self.verticalLayout_3.addWidget(self.label_xmin)
        self.lineEdit_xmin = QtGui.QLineEdit(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEdit_xmin.sizePolicy().hasHeightForWidth())
        self.lineEdit_xmin.setSizePolicy(sizePolicy)
        self.lineEdit_xmin.setObjectName(_fromUtf8("lineEdit_xmin"))
        self.verticalLayout_3.addWidget(self.lineEdit_xmin)
        self.label_xmax_2 = QtGui.QLabel(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Minimum, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.label_xmax_2.sizePolicy().hasHeightForWidth())
        self.label_xmax_2.setSizePolicy(sizePolicy)
        self.label_xmax_2.setObjectName(_fromUtf8("label_xmax_2"))
        self.verticalLayout_3.addWidget(self.label_xmax_2)
        self.lineEdit_xmax = QtGui.QLineEdit(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEdit_xmax.sizePolicy().hasHeightForWidth())
        self.lineEdit_xmax.setSizePolicy(sizePolicy)
        self.lineEdit_xmax.setObjectName(_fromUtf8("lineEdit_xmax"))
        self.verticalLayout_3.addWidget(self.lineEdit_xmax)
        self.label_binsize = QtGui.QLabel(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Preferred, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.label_binsize.sizePolicy().hasHeightForWidth())
        self.label_binsize.setSizePolicy(sizePolicy)
        self.label_binsize.setObjectName(_fromUtf8("label_binsize"))
        self.verticalLayout_3.addWidget(self.label_binsize)
        self.lineEdit_binsize = QtGui.QLineEdit(self.tab)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.lineEdit_binsize.sizePolicy().hasHeightForWidth())
        self.lineEdit_binsize.setSizePolicy(sizePolicy)
        self.lineEdit_binsize.setObjectName(_fromUtf8("lineEdit_binsize"))
        self.verticalLayout_3.addWidget(self.lineEdit_binsize)
        spacerItem5 = QtGui.QSpacerItem(20, 40, QtGui.QSizePolicy.Minimum, QtGui.QSizePolicy.Expanding)
        self.verticalLayout_3.addItem(spacerItem5)
        self.pushButton_unit2theta = QtGui.QPushButton(self.tab)
        self.pushButton_unit2theta.setObjectName(_fromUtf8("pushButton_unit2theta"))
        self.verticalLayout_3.addWidget(self.pushButton_unit2theta)
        self.pushButton_unitD = QtGui.QPushButton(self.tab)
        self.pushButton_unitD.setObjectName(_fromUtf8("pushButton_unitD"))
        self.verticalLayout_3.addWidget(self.pushButton_unitD)
        self.pushButton_unitQ = QtGui.QPushButton(self.tab)
        self.pushButton_unitQ.setObjectName(_fromUtf8("pushButton_unitQ"))
        self.verticalLayout_3.addWidget(self.pushButton_unitQ)
        spacerItem6 = QtGui.QSpacerItem(20, 40, QtGui.QSizePolicy.Minimum, QtGui.QSizePolicy.Expanding)
        self.verticalLayout_3.addItem(spacerItem6)
        self.horizontalLayout_5.addLayout(self.verticalLayout_3)
        self.verticalLayout_2.addLayout(self.horizontalLayout_5)
        self.gridLayout_2.addLayout(self.verticalLayout_2, 0, 0, 1, 1)
        self.tabWidget.addTab(self.tab, _fromUtf8(""))
        self.tab_2 = QtGui.QWidget()
        self.tab_2.setObjectName(_fromUtf8("tab_2"))
        self.tabWidget.addTab(self.tab_2, _fromUtf8(""))
        self.horizontalLayout_2.addWidget(self.tabWidget)
        self.verticalLayout.addLayout(self.horizontalLayout_2)
        self.gridLayout.addLayout(self.verticalLayout, 0, 0, 1, 1)
        MainWindow.setCentralWidget(self.centralwidget)
        self.menubar = QtGui.QMenuBar(MainWindow)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 1200, 25))
        self.menubar.setObjectName(_fromUtf8("menubar"))
        self.menuFile = QtGui.QMenu(self.menubar)
        self.menuFile.setObjectName(_fromUtf8("menuFile"))
        self.menuView = QtGui.QMenu(self.menubar)
        self.menuView.setObjectName(_fromUtf8("menuView"))
        self.menuHelp = QtGui.QMenu(self.menubar)
        self.menuHelp.setObjectName(_fromUtf8("menuHelp"))
        self.menuWindow = QtGui.QMenu(self.menubar)
        self.menuWindow.setObjectName(_fromUtf8("menuWindow"))
        MainWindow.setMenuBar(self.menubar)
        self.statusbar = QtGui.QStatusBar(MainWindow)
        self.statusbar.setObjectName(_fromUtf8("statusbar"))
        MainWindow.setStatusBar(self.statusbar)
        self.actionQuit = QtGui.QAction(MainWindow)
        self.actionQuit.setObjectName(_fromUtf8("actionQuit"))
        self.menuFile.addAction(self.actionQuit)
        self.menubar.addAction(self.menuFile.menuAction())
        self.menubar.addAction(self.menuView.menuAction())
        self.menubar.addAction(self.menuWindow.menuAction())
        self.menubar.addAction(self.menuHelp.menuAction())

        self.retranslateUi(MainWindow)
        self.tabWidget.setCurrentIndex(0)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(_translate("MainWindow", "MainWindow", None))
        self.label_expNo.setText(_translate("MainWindow", "Exp No", None))
        self.label_scanNo.setText(_translate("MainWindow", "Scan No", None))
        self.pushButton_loadData.setText(_translate("MainWindow", "Load Data", None))
        self.label_calibration.setText(_translate("MainWindow", "Ge 113 IN Config", None))
        self.label_normalizeMonitor.setText(_translate("MainWindow", "Normalization Monitor", None))
        self.label_outputFormat.setText(_translate("MainWindow", "Save As", None))
        self.pushButton_saveData.setText(_translate("MainWindow", "Save", None))
        self.label_detExcluded.setText(_translate("MainWindow", "Detectors to Exclude", None))
        self.label_wavelength.setText(_translate("MainWindow", "Wavelength", None))
        self.label_xmin.setText(_translate("MainWindow", "Minimum X", None))
        self.label_xmax_2.setText(_translate("MainWindow", "Maximum X", None))
        self.label_binsize.setText(_translate("MainWindow", "Bin Size", None))
        self.pushButton_unit2theta.setText(_translate("MainWindow", "2theta", None))
        self.pushButton_unitD.setText(_translate("MainWindow", "dSpacing", None))
        self.pushButton_unitQ.setText(_translate("MainWindow", "Q", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab), _translate("MainWindow", "Normalized", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_2), _translate("MainWindow", "Advanced Setup", None))
        self.menuFile.setTitle(_translate("MainWindow", "File", None))
        self.menuView.setTitle(_translate("MainWindow", "View", None))
        self.menuHelp.setTitle(_translate("MainWindow", "Help", None))
        self.menuWindow.setTitle(_translate("MainWindow", "Window", None))
        self.actionQuit.setText(_translate("MainWindow", "Quit", None))
        self.actionQuit.setShortcut(_translate("MainWindow", "Ctrl+Q", None))

