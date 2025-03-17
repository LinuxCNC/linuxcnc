import os
import sys
import getopt

from PyQt5.QtWidgets import (QMainWindow, QToolBar, QMessageBox,
        QTreeWidget, QTreeWidgetItem, QStyle, QFileDialog,
        QTextEdit, QMenu, QAbstractItemView)
from PyQt5 import uic
from PyQt5.QtCore import QObject, QTimer, QUrl, Qt, QSize
from PyQt5.QtGui import QColor, QIcon, QFont, QImage

import gcode
# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

from qtvcp.lib.native_cam.qtncam import NCam

from qtvcp.lib.native_cam.message import ZMQMessage
from qtvcp.lib.native_cam.custom_widgets import (MetaClass, TreeItem,
 tv_select, ComboDelegate, treeModel, ToolButton, IconView)
from qtvcp.lib.native_cam.qt_dialogs import *
from qt5_graphics import Lcnc_3dGraphics
from qtvcp.core import Info, Status

INFO = Info()
STATUS = Status()
############################################################

GENERATED_FILE = "ncam.ngc" #TODO from qtnccam file, Does it change?
SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int', 'gc-lines',
                        'tool', 'gcode', 'text', 'list', 'float', 'string', 'engrave',
                        'combo', 'combo-user', 'items', 'filename', 'prjname']

current_dir =  os.path.dirname(__file__)

###############
# Window
###############
class NCamWindow(QMainWindow, NCam):

    def __init__(self, parent=None):
        super(NCamWindow, self).__init__(parent)
        try:
            self.filename = os.path.abspath(os.path.join(current_dir, 'ncam.ui'))
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

        self._model = treeModel()
        self._model.dataChanged.connect(lambda :self.update_do_btns(True))
        self._model.iconBasePath = os.path.join(current_dir, 'graphics')

        tv = self.treeView

        # for combobox in treeview
        delegate = ComboDelegate(tv)
        tv.setItemDelegate(delegate)

        tv.setContextMenuPolicy(Qt.CustomContextMenu)
        tv.customContextMenuRequested.connect(self.openMenu)

        tv.setModel(self._model)
        tv.setAlternatingRowColors(True)
        tv.clicked.connect(self.get_selected_feature)
        tv.clicked.connect(self.adjTreeBranch)
        tv.expanded.connect(self.expandCalled)

        # select items rather then rows
        tv.setSelectionBehavior(QAbstractItemView.SelectItems)

        tv.SelectionMode(QAbstractItemView.SingleSelection)

        # for reloads in qtvcp screens
        self.zMessanger = ZMQMessage()

        # initialize ncam
        self.init()

        self.actionOutputWindow.triggered.connect(lambda s :self.toggleOutputWindow(s))

    # expand or collapse current selection
    def adjTreeBranch(self,path):
        if self.treeView.isExpanded(self.treeView.currentIndex()):
            self.treeView.collapse(self.treeView.currentIndex())
        else:
            self.treeView.expand(self.treeView.currentIndex())

    def expandCalled(self, index):
        self.treeView.scrollTo(index,QAbstractItemView.PositionAtTop)

    def toggleOutputWindow(self, state):
        self.stackedWidget_output.setCurrentIndex(int(state))
        if state:
            self.actionOutputWindow.setText('Graphics')
        else:
            self.actionOutputWindow.setText('Gcode text')
############
# Gcode Display
############

    def addDisplayWidget(self):
        self.emptypath = os.path.join( "/tmp/LINUXCNCtempEmpty.ngc")

        emptyfile = open(self.emptypath, "w")
        print(("m2"), file=emptyfile)
        emptyfile.close()

        inipath = STATUS.stat.ini_filename
        if inipath == '':
            inipath = "{}/configs/sim/qtdragon/qtdragon_metric.ini".format(current_dir)
        print ('path:',inipath)
        self.graphics = Lcnc_3dGraphics(inipath=None)
        self.setGraphicsDisplay()

        self.loadDisplay(self.emptypath)

        self.displayLayout.addWidget(self.graphics)
        self._displayToolbar = QToolBar()

        self._displayToolbar.addAction(self.actionShowP)
        self.actionShowP.setIcon(QIcon(os.path.join(current_dir, 'graphics/tool_axis_p.gif')))
        self.actionShowP.triggered.connect(lambda s :self.setDisplayView('p'))

        if INFO.MACHINE_IS_LATHE:
            self._displayToolbar.addAction(self.actionShowY)
            self.actionShowY.setIcon(QIcon(os.path.join(current_dir, 'graphics/tool_axis_y.gif')))
            self.actionShowY.triggered.connect(lambda s :self.setDisplayView('y'))

            self._displayToolbar.addAction(self.actionShowY2)
            self.actionShowY2.setIcon(QIcon(os.path.join(current_dir, 'graphics/tool_axis_y2.png')))
            self.actionShowY2.triggered.connect(lambda s :self.setDisplayView('y2'))

        else:
            self._displayToolbar.addAction(self.actionShowZ)
            self.actionShowZ.setIcon(QIcon(os.path.join(current_dir, 'graphics/tool_axis_z.gif')))
            self.actionShowZ.triggered.connect(lambda s :self.setDisplayView('z'))

        self.displayLayout.setMenuBar(self._displayToolbar)

    def setGraphicsDisplay(self):
        # class patch to catch gcode errors - in theory
        self.graphics.report_gcode_error = self.report_gcode_error
        # reset traverse color so other displays don't change
        self.defaultColor = self.graphics.colors['traverse']
        self.graphics.current_view = 'p'
        self.graphics.metric_units = INFO.MACHINE_IS_METRIC
        self.graphics.use_gradient_background = True
        self.graphics.show_tool = False
        self.graphics.grid_size = 2
        #self.graphics.cancel_rotate = True

    def loadDisplay(self,fn):
        print('Load Display from:',fn)
        self.graphics.colors['traverse'] = (0.0, 1.0, 0.0)
        self.graphics.load(fn)
        self.graphics.colors['traverse'] = self.defaultColor

    def setDisplayView(self, view = 'z'):
        #self.loadDisplay(self.emptypath)
        self.graphics.current_view = view
        self.graphics.set_current_view()

    # class patch
    ############
    def report_gcode_error(self, result, seq, filename):
        error_str = gcode.strerror(result)
        errortext = "G-Code error in " + os.path.basename(filename) + "\n" + "Near line " \
                    + str(seq) + " of\n" + filename + "\n" + error_str + "\n"
        mess_dlg(errortext,title = 'Gcode Error Detected',icon=WARNING)

    def updateDisplay(self):
        self.graphics.colors['traverse'] = (0.0, 1.0, 0.0)
        self.workfile = open(self.workpath, "w")

        ###############################
        # Setup
        ###############################
        print((''), file=self.workfile)

        ##############################
        # cleanup
        ##############################
        #print(('m2'), file=self.workfile)

        self.workfile.close()
        self.loadDisplay(self.workpath)
        self.graphics.colors['traverse'] = self.defaultColor

##################################################
# Action menu/toolbar setup
##################################################
    def create_actions(self):
        def ca(act, actionname, iconName, label, accel, tooltip, callback, arg=None):
            #print(arg)
            act.setToolTip(tooltip)

            if not label is None:
                act.setText(label)

            try:
                if not iconName is None:
                    if isinstance(iconName, str):
                        try:
                            icon = QIcon('{}/{}'.format(self.DATA.GRAPHICS_DIR,iconName))

                        except:
                            icon = QIcon.fromTheme(iconName)
                    else:
                        icon = self.style().standardIcon(iconName)
                    act.setIcon(icon)
            except:
                print('bad icon name')

            if callback is not None :
                if arg is None:
                    act.triggered.connect(callback)
                else:
                    act.triggered.connect(lambda s, arg=arg: callback(arg))

            if accel is not None:
                pass

        def cmenu(menu, menuname, iconName, label, accel, tooltip, callback, *args):
            menu.setToolTip(tooltip)
            menu.setTitle(label)

        # actions related to projects_("Create a New Project")("Open A Project")_("Open a Saved Project xml file")_('Save Project')
        # "<control>X"
        cmenu(self.menuProject,'Project', None, _("_Projects"), None, None, None)
        ca(self.actionNew,"New", QStyle.SP_FileIcon, None, "<control>N", None, self.action_new_project)
        ca(self.actionOpen,"Open", QStyle.SP_DirIcon, None, "<control>O", None, self.action_open_project, 0)
        ca(self.actionOpenExample,"OpenExample", None, _('Open Example'), '', _('Open Example Project'), self.action_open_project, 1)
        ca(self.actionSave,"Save",QStyle.SP_DialogSaveButton, None, "<control>S", _("Save project as xml file"), self.action_save_project)
        ca(self.actionSaveTemplate,"SaveTemplate", None, _('Save as Default Template'), '', _("Save project as default template"), self.action_save_template)
        ca(self.actionSaveNGC,"SaveNGC", None, _('Export gcode as RS274NGC'), '', _('Export gcode as RS274NGC'), action_save_ngc)
        # actions related to editing
        #self.actionEditMenu = ca("EditMenu", None, _("_Edit"), None, None, self.edit_menu_activate)
        ca(self.actionUndo ,"Undo", QStyle.SP_ArrowBack, None, "<control>Z", _('Undo last operation'), self.action_undo)
        ca(self.actionRedo ,"Redo", QStyle.SP_ArrowForward, None, "<control><shift>Z", _('Cancel last Undo'), self.action_redo)

        ca(self.actionCut,"Cut", 'gtk.STOCK_CUT', None, "<control>X", _('Cut selected subroutine to clipboard'), self.action_cut)
        ca(self.actionCopy,"Copy", 'gtk.STOCK_COPY', None, "<control>C", _('Copy selected subroutine to clipboard'), self.action_copy)
        ca(self.actionPaste,"Paste", 'gtk.STOCK_PASTE', None, "<control>V", _('Paste from clipboard'), self.action_paste)
        ca(self.actionAdd,"Add", 'action-add.gif', None, "<control>Insert", _('Add a subroutine'), self.action_add)
        ca(self.actionDuplicate,"Duplicate", 'gtk.STOCK_COPY', _('Duplicate'), "<control>D", _('Duplicate selected subroutine'), self.action_duplicate)
        ca(self.actionDelete,"Delete", 'action-delete.gif', None, "<control>Delete", _('Remove selected subroutine'), self.action_delete)
        ca(self.actionAppendItm,"AppendItm", QStyle.SP_MediaSeekBackward, _("Add to Items"), "<control>Right", _("Add to Items"), self.action_appendItm)
        ca(self.actionRemoveItm,"RemoveItm", QStyle.SP_TrashIcon, _("Remove from Items"), "<control>Left", _('Remove from Items'), self.action_removeItem)
        ca(self.actionMoveUp,"MoveUp", QStyle.SP_ArrowUp, _('Move up'), "<control>Up", _('Move up'), self.moveItem, -1)
        ca(self.actionMoveDown,"MoveDown", QStyle.SP_ArrowDown, _('Move down'), "<control>Down", _('Move down'), self.moveItem, 1)
        ca(self.actionSaveUser,"SaveUser", QStyle.SP_DialogSaveButton, _('Save Values as Defaults'), '', _('Save Values of this Subroutine as Defaults'), self.action_saveUser)
        ca(self.actionDeleteUser,"DeleteUser", QStyle.SP_TitleBarCloseButton, _("Delete Custom Default Values"), None, _("Delete Custom Default Values"), self.action_deleteUser)

        # actions related to adding subroutines
        ca(self.actionLoadCfg,"LoadCfg", QStyle.SP_DirIcon, _('Add a Prototype Subroutine'), '', _('Add a Subroutine Definition File'), self.action_loadCfg)
        ca(self.actionImportXML,"ImportXML", QStyle.SP_DialogOpenButton, _('Import a Project File'), None, _('Import a Project Into the Current One'), self.action_importXML)

        # actions related to view
        ca(self.actionViewMenu,"ViewMenu", None, _("_View"), None, None, self.view_menu_activate)
        ca(self.actionCollapse,"Collapse", 'gtk.STOCK_ZOOM_OUT', _("Collapse All Other Nodes"), '<control>K', _("Collapse All Other Nodes"), self.action_collapse)
        ca(self.actionSaveLayout,"SaveLayout", 'gtk.STOCK_SAVE', _('Save As Default Layout'), '', _('Save As Default Layout'), self.action_saveLayout)

        #ca(self.action_group.add_radio_actions([
        ca(self.actionSingleView, "SingleView", None, _('Single View'), None, None, self.set_layout)
        ca(self.actionDualView, "DualView", None,  _('Dual Views'), None, None, self.set_layout)

        #], 1, self.set_layout)

        #ca(self.action_group.add_radio_actions([
        #    ("TopBottom", None, _('Top / Bottom Layout'), None, None, 1),
        #    ("SideSide", None, _('Side By Side Layout'), None, None, 2)
        #], 1, self.set_layout)

        #ca(self.actionHideCol = gtk.ToggleAction("HideCol", _('Master Value Column Hidden'), _('In master treeview'), None)
        #ca(self.actionHideCol.connect("toggled", self.set_layout)
        #ca(self.action_group.add_action(self.actionHideCol)

        #ca(self.actionSubHdrs = gtk.ToggleAction("SubHdrs", _('Sub-Groups In Master Tree'), _('Sub-Groups In Master Tree'), None)
        #ca(self.actionSubHdrs.connect("toggled", self.set_layout)
        #ca(self.action_group.add_action(self.actionSubHdrs)

        # actions related to utilities
        self.actionUtilMenu.aboutToShow.connect(self.utilMenu_activate)
        ca(self.actionLoadTools,"LoadTools", QStyle.SP_BrowserReload, _("Reload Tool Table"), None, _("Reload Tool Table"), self.action_loadToolTable)
        ca(self.actionPreferences,"Preferences", 'gtk.STOCK_PREFERENCES', _("Edit Preferences"), None, _("Edit Preferences"), self.action_preferences)

        ca(self.actionAutoRefresh, "AutoRefresh",None, _("Auto-refresh"),None, _('Auto-refresh LinuxCNC'), None)
        self.actionAutoRefresh.setChecked(False)
        #ca(self.action_group.add_action(self.actionAutoRefresh)

        ca(self.actionDebugPrint,"Debugging", None, _("Debug Print XML"), None, _("Debug Print XML"), self.action_debug_print)

        ca(self.actionChUnits,"ChUnits", None, _("Change Units"), None, _(""), self.action_chUnits)

        # actions related to validations
        cmenu(self.menuValidation,"menuValidation", 'gtk.STOCK_INFO', _("_Validation Messages"), None, None, self.validation_menu_activate)
        ca(self.actionValAllDlg,"ValAllDlg", 'gtk.STOCK_YES', _("Show All"), None, _("Show All Non-validation Messages"), self.action_ValAllDlg)
        ca(self.actionValNoDlg,"ValNoDlg", 'gtk.STOCK_NO', _("Show None"), None, _("Do Not Show Any Messages"), self.action_ValNoDlg)
        ca(self.actionValFeatDlg,"ValFeatDlg", 'gtk.STOCK_YES', _("Show All For Current Type"), None, None, self.action_ValFeatDlg)
        ca(self.actionValFeatNone,"ValFeatNone", 'gtk.STOCK_NO', _("Show None For Current Type"), None, None, self.action_ValFeatNone)

        # actions related to help
        cmenu(self.menuHelp,"HelpMenu", None, _("_Help"), None, None, None)
        ca(self.actionYouTube,"YouTube", None, _('NativeCAM on YouTube'), None, None, self.action_youTube)
        #ca(self.actionYouTrans,"YouTranslate", None, _('Translating NativeCAM'), None, None, self.action_youTrans)
        ca(self.actionCNCHome,"CNCHome", None, _("LinuxCNC web Site"), None, None, self.action_lcncHome)
        ca(self.actionForum,"CNCForum", None, _('LinuxCNC Forum'), None, None, self.action_lcncForum)
        ca(self.actionAbout,"About", 'gtk.STOCK_ABOUT', None, None, None, self.action_about)

        # actions related to toolbars and popup
        ca(self.actionHideField,"HideField", None, _("Hide Selected Field"), None, _("Hide Selected Field"), self.action_hideField)
        ca(self.actionShowF,"ShowFields", None, _("Show All Fields"), None, _("Show All Fields"), self.action_showFields)
        ca(self.actionCurrent,"Current", QStyle.SP_FileDialogStart, _("Save Project as Current Work"), '', _('Save Project as Current Work'), self.action_saveCurrent)
        ca(self.actionBuild,"Build", 'build.png', _('Generate %(filename)s') % {'filename':self.DATA.GENERATED_FILE}, None,
                     _('Generate %(filename)s and load it in LinuxCNC') % {'filename':self.DATA.GENERATED_FILE}, self.action_build)
        ca(self.actionRename,"Rename", None, _("Rename Subroutine"), None, _('Rename Subroutine'), self.action_renameF)
        ca(self.actionChngGrp,"ChngGrp", None, _("Group <-- --> Sub-group"), None, _('Group <-- --> Sub-group'), self.action_chng_group)
        ca(self.actionDataType,"DataType", None, _("Change to GCode"), None, _('Change to GCode'), self.action_gcode)
        ca(self.actionRevertType,"RevertType", None, _("Revert to original type"), None, _('Revert to original type'), self.action_revert_type)

        ca(self.actionBack,None, 'upper-level-icon.png', _("Back"), None, None, None)
        ca(self.actionWindowClose,None, 'window-close.png', _("Back"), None, None, self.btn_cancel_add)

    def addToolbarWidgets(self, toolbar, items):
        ''' add actions/buttons to a toolbar
        '''
        if items == 'separator' :
            toolbar.addSeparator()
        else :
            if items[3] is not None :
                icon = QIcon('{}/{}'.format(self.DATA.GRAPHICS_DIR,items[3]))
                action = toolbar.addAction(icon, items[0])
            else :
                action = toolbar.addAction(items[0])
            if items[1] is not None :
                action.setToolTip(_(items[1]))
            action.triggered.connect( lambda s, items=items: self.add_feature(None,items[2]))

    # right click context menu
    def openMenu(self, position):
    
        indexes = self.treeView.selectedIndexes()
        if len(indexes) > 0:
        
            level = 0
            index = indexes[0]
            while index.parent().isValid():
                index = index.parent()
                level += 1
        
        menu = QMenu()
        if level == 0:
            menu.addAction(self.actionRename)
            menu.addAction(self.actionDelete)
            menu.addAction(self.actionUndo)
            menu.addAction(self.actionRedo)
            menu.addAction(self.actionHideField)
            menu.addAction(self.actionShowF)
            menu.addAction(self.actionAdd)
            menu.addAction(self.actionDuplicate)
#            menu.addAction()
            menu.addAction(self.actionMoveUp)
            menu.addAction(self.actionMoveDown)
            menu.addAction(self.actionAppendItm)
            menu.addAction(self.actionRemoveItm)

        elif level == 1:
            menu.addAction(self.tr("Edit object/container"))
        elif level == 2:
            menu.addAction(self.tr("Edit object"))
        
        menu.exec_(self.treeView.viewport().mapToGlobal(position))

    def add_timer(self, time, function):
        '''
        Timer for auto refresh
        '''
        self.timer = timer = QTimer()
        timer.setSingleShot(True)
        timer.timeout.connect(function)
        timer.start(time)

##########################
# add menu
##########################
    def build_add_menu(self):
        self.iconView = IconView(self)
        self.iconViewVerticalLayout.addWidget(self.iconView)

        self.add_catalog_items(self.actionMenuAdd)
        self.iconView.showTopList(self.actionMenuAdd)

    def add_catalog_items(self, menu_add):
        self._row = self._col = 0

        def add_to_menu(grp_menu, path) :

            for ptr in range(len(path)) :
                try :
                    p = path[ptr]
                    if p.tag.lower() in ["menu", "menuitem", "group", "sub"] :
                        name = p.get("name") if "name" in p.keys() else ""
                        icon = p.get('icon')
                        iconPath =  '{}/{}'.format(self.DATA.GRAPHICS_DIR, icon)
                        tooltip = _(p.get("tool_tip")) if "tool_tip" in p.keys() else None
                        src = p.get('src')

                        if p.tag.lower() in ['menu', "group"] :
                            sub = self.actionMenuAdd.addMenu(QIcon(iconPath), name)
                            add_to_menu(sub, p)
                        else:
                            act = grp_menu.addAction(QIcon(iconPath),name)
                            act.triggered.connect( lambda s, src=src: self.add_feature(None,src))

                    elif p.tag.lower() == "separator":
                        grp_menu.addSeparator()
                except Exception as e:
                    print(e)

        if self.catalog.tag != 'ncam_ui' :
            mess_dlg(_('Menu is old format, no toolbar defined.\nUpdate to new format'))
            add_to_menu(menu_add, self.catalog)
        else :
            for _ptr in range(len(self.catalog)) :
                _p = self.catalog[_ptr]
                if _p.tag.lower() in ["menu", "group"] :
                    add_to_menu(menu_add, _p)

    def showIconView(self):
        self.stackedWidget.setCurrentIndex(1)
        self.menubar.setEnabled(False)
        self.main_toolbar.setEnabled(False)
        self.nc_toolbar.setEnabled(False)

    def hideIconView(self):
        self.stackedWidget.setCurrentIndex(0)
        self.menubar.setEnabled(True)
        self.main_toolbar.setEnabled(True)
        self.nc_toolbar.setEnabled(True)

##########################
#
##########################


    def get_selected_feature(self, widget=None) :
        '''
        from the selected item, define:

        self.iter_selected_type
        self.items_ts_parent_s
        self.items_path
        self.items_lpath
        self.iter_selected_type
        self.items_ts_parent_s
        self.selected_param
        self.selected_feature 
        self.selected_feature_itr
        self.feature_ts_path
        self.selected_feature_ts_path_s
        self.iter_next
        self.can_move_down
        self.can_add_to_group
        self.can_remove_from_group
        self.selected_feature_parent_itr
        '''
        print('get selected feature:',widget)
        model = self.treeView.model()
        old_selected_feature = self.selected_feature

        obj = self.treeView.selectedIndexes()
        print('selected indexes:',obj)
        selectedItem = model.getItemFromIndex(obj)
        print('sel=',selectedItem)

        if selectedItem is not None :
            selectedIndex = obj[0]
            print('Selected Row',selectedIndex.row())
            print(selectedIndex.parent().row(), selectedIndex)
            print('Path', model.indexToPath(selectedIndex))


            self.selected_type = selectedItem.meta.xml.get_type()
            self.status(selectedItem.meta.get_tooltip())
            #print(selectedItem.meta.xml)
            print('selected type',self.selected_type)
            # convert sorted to original index tree sorted)
            #ts_itr = model.convert_iter_to_child_iter(itr)
            sortedIndex = selectedIndex #TODO
            self.selected_param = None

            # convert 'selected type' (str) to selected itr (int)
            if self.selected_type == "items" :
                self.iter_selected_type = tv_select.items

                self.items_path = int(selectedItem.findPath())
                # string representation of the gtk iter ie 1:2:0
                self.items_ts_parent_s = model.indexToPath(selectedIndex)

                n_children = selectedItem.childCount()
                print('item Path:',self.items_path, n_children)
                self.items_lpath = (self.items_path + n_children)

            elif self.selected_type in ["header", 'sub-header'] :
                self.iter_selected_type = tv_select.header
                self.selected_param = sortedIndex

            elif self.selected_type in SUPPORTED_DATA_TYPES :
                self.iter_selected_type = tv_select.param
                self.selected_param = sortedIndex

            else :
                self.iter_selected_type = tv_select.feature

            # if selected is items or header....
            if self.iter_selected_type in [tv_select.items, tv_select.header] :
                items_ts_path =  model.indexToPath(selectedIndex)
                #sortedIndex = self.treestore.iter_parent(sortedIndex)

                # string representation of the gtk iter ie 1:2:0
                self.items_ts_parent_s = items_ts_path

            # find top parent item of selection?
            itemParent = selectedItem
            while itemParent.meta.get_type() in SUPPORTED_DATA_TYPES :
                # get patent of child
                itemParent = itemParent.parent()

            self.selected_feature_itr = itemParent # selected Item object
            self.selected_feature = itemParent.getFeature() # selected Item Feature (XML)

            #sortedIndex = model.convert_iter_to_child_iter(itemParent)
            self.selected_feature_sortedIndex = self.selected_feature #sortedIndex

            self.selected_feature_ts_itr = itemParent

            print('selected find path',itemParent)
            self.feature_ts_path = model.indexToPath(selectedIndex)

            self.selected_feature_ts_path_s = model.indexToPath(selectedIndex)

            # find next lower sibling in menu
            self.iter_next = itemParent.nextSibling()
            print('next sibling',self.iter_next)
            if self.iter_next:
                self.can_move_down = (self.iter_selected_type == tv_select.feature)
                s = self.iter_next.meta.get_type()
                self.can_add_to_group = ('type="items"' in s) and \
                        (self.iter_selected_type == tv_select.feature)
            # no lower sibling
            else :
                self.can_add_to_group = False
                self.can_move_down = False

            self.printSelected()

            self.selected_feature_parent_itr = itemParent.parent()
            print(self.selected_feature_parent_itr)
            if not self.selected_feature_parent_itr.atTop():
                #path_parent = model.get_path(self.selected_feature_parent_itr)
                print(itemParent.parent(),itemParent)
                self.can_remove_from_group = (self.iter_selected_type == tv_select.feature) and \
                    self.selected_feature_parent_itr.meta.get_type() == "items"
            else :
                path_parent = None
                self.can_remove_from_group = False

            self.selected_feature_path = itemParent
            self.can_move_up = itemParent.canMoveUp() and \
                (self.iter_selected_type == tv_select.feature)

            #if index_s :
            #    if path_parent is None :
            #        path_previous = (index_s - 1,)
            #    else :
            #        path_previous = path_parent[0: depth - 1] + (index_s - 1,)
            #    self.iter_previous = model.get_iter(path_previous)
            #else :
            #    self.iter_previous = None

            self.printSelected('end')
        else:
            self.iter_selected_type = tv_select.none
            self.selected_feature = None
            self.selected_type = 'xxx'
            self.can_move_up = False
            self.can_move_down = False
            self.can_add_to_group = False
            self.can_remove_from_group = False
            self.items_lpath = None
            tree_path = None
            self.status('')

        if self.actionDualView.isChecked() :
            if self.iter_selected_type == tv_select.none :
                if self.treeview2 is None:
                    self.create_second_treeview()
                else :
                    self.treeview2.set_model(None)

            if ((old_selected_feature == self.selected_feature) and \
                (self.iter_selected_type in [tv_select.items, tv_select.feature, tv_select.header])) \
                    or (old_selected_feature != self.selected_feature) :

                if self.iter_selected_type in [tv_select.items, tv_select.header] :
                    a_filter = self.treestore.filter_new(items_ts_path)
                else :
                    a_filter = self.treestore.filter_new(self.feature_ts_path)
                a_filter.setVisible_column(3)
                self.details_filter = a_filter

                self.treeview2.set_model(self.treestore)
                self.treeview2.set_model(self.details_filter)
                self.treeview2.expand_all()

        #if tree_path is not None :
        #    self.treeview.expand_to_path(tree_path + (0, 0))
        self.can_delete_duplicate = (self.iter_selected_type == tv_select.feature)
        self.set_actions_sensitives()

    def set_actions_sensitives(self):
        self.actionCollapse.setEnabled(self.selected_feature is not None)

        self.actionSave.setEnabled(self.selected_feature is not None)
        self.actionSaveTemplate.setEnabled(self.selected_feature is not None)
        self.actionSaveNGC.setEnabled(self.selected_feature is not None)

        self.actionSaveUser.setEnabled(self.selected_feature is not None)
        self.actionDeleteUser.setEnabled((self.selected_feature is not None) and \
                    (self.selected_feature.get_type() in self.USER_SUBROUTINES))

        self.actionDelete.setEnabled(self.can_delete_duplicate)
        self.actionDuplicate.setEnabled(self.can_delete_duplicate)
        self.actionMoveUp.setEnabled(self.can_move_up)
        self.actionMoveDown.setEnabled(self.can_move_down)
        self.actionAppendItm.setEnabled(self.can_add_to_group)
        self.actionRemoveItm.setEnabled(self.can_remove_from_group)
        self.actionCut.setEnabled(self.can_delete_duplicate)
        self.actionCopy.setEnabled(self.can_delete_duplicate)

        self.actionChngGrp.setVisible(self.selected_type in ["sub-header", "header"] and \
                            self.actionDualView.isChecked())
        #self.actionSetDigits.setVisible(self.selected_type == 'float')
        self.actionDataType.setVisible(self.selected_type in ['float', 'int'])
        self.actionRevertType.setVisible(self.selected_type == 'gcode')

        self.actionRename.setVisible(self.iter_selected_type == tv_select.feature)

        self.actionHideField.setEnabled((self.selected_type in SUPPORTED_DATA_TYPES) and \
                                      (self.selected_type != 'items'))
        self.actionShowF.setEnabled(self.selected_feature is not None and \
                                       self.selected_feature.has_hidden_fields())

###################
# create safety M123 code
###################
    def create_M_file(self, NCAM_DIR, NGC_DIR, CATALOGS_DIR, GRAPHICS_DIR) :
        p = os.path.join(NCAM_DIR, NGC_DIR, 'M123')
        print('make M123 at {}'.format(p))
        with open(p, 'wt') as f :


            f.write('#!/usr/bin/env python3\n# coding: utf-8\n')
            f.write('import os, sys\n')
            f.write('from PyQt5.QtWidgets import QMessageBox, QTextEdit, QApplication\n')
            f.write('from PyQt5.QtCore import Qt\n')
            f.write('from PyQt5.QtGui import QPixmap\n')
            f.write('app = QApplication(sys.argv)\n')
            f.write("fname = '%s'\n" % os.path.join(NCAM_DIR, CATALOGS_DIR, 'no_skip_dlg.conf'))
            f.write('if os.path.isfile(fname) :\n    exit(0)\n\n')
            f.write("msg = '%s'\n" % _('1) Stop this LinuxCNC program,\\n2) toggle the optional stop button\\n3) rerun this program'))
            f.write("title = '%s'\n" % _('Optional block not active'))
            f.write("info = '%s'\n" % _('Pressing abort will stop this program'))
            f.write("iconName = '%s'\n\n" % os.path.join(NCAM_DIR, GRAPHICS_DIR, 'skip_block.png'))

            f.write('''
def forceDetailsOpen(dlg):
  try:
    # force the details box open on first time display
    for i in dlg.buttons():
        if dlg.buttonRole(i) == QMessageBox.ActionRole:
            for j in dlg.children():
                for k in j.children():
                    if isinstance( k, QTextEdit):
                        if not k.isVisible():
                            i.click()
  except:
    pass

dlg = QMessageBox()
dlg.setTextFormat(Qt.RichText)
dlg.setText('<b>{}</b>'.format(title))
dlg.setInformativeText(info)
dlg.setDetailedText(msg)
dlg.setStandardButtons(QMessageBox.Abort)
dlg.addButton('Do not ask again', QMessageBox.YesRole)
dlg.setIconPixmap(QPixmap(iconName))
dlg.setWindowFlags(dlg.windowFlags() | Qt.WindowStaysOnTopHint)
dlg.show()
forceDetailsOpen(dlg)

button = dlg.exec()

if button == QMessageBox.Abort:
    exit(1)
else:
    open(fname, 'w').close()
    exit(1)

''')
        #f.write("if cb.get_active() :\n    open(fname, 'w').close()\n")
        os.chmod(p, 0o755)
        mess_dlg(_('LinuxCNC needs to be restarted now'))

#############
# statusbar
###############
    def status(self, text):
        self.statusbar.showMessage(text)

#############
# Dummy
#############
    def create_treeview(self):
        return

#################
# Debug code
#################

    def action_debug_print(self):
        print(self.selected_feature)

    def printItemName(self, item):
        try:
            name = item.meta.find_attr('name')
        except:
            return 'None'
        return name

    def printSelected(self, title = ''):
        return
        print('{}'.format(title))
        methods = ('iter_selected_type',
            'selected_feature',
            'selected_type',
            'can_move_up',
            'can_move_down',
            'can_add_to_group',
            'can_remove_from_group',
            'items_lpath',
            'selected_feature_ts_path_s',)

        for i in methods:
            print('{}:'.format(i.upper()),self[i])

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

#########################
# global functions
#########################

def verify_ini(fname, ctlog, in_tab) :
    path2ui = os.path.join(SYS_DIR, 'ncam.ui')
    req = '# required NativeCAM item :\n'

    with open(fname, 'r') as b :
        txt = b.read()
    if (path2ui not in txt) or ('my-stuff' not in txt) :
        if not os.path.exists(fname + '.bak') :
            with open(fname + '.bak', 'w') as b :
                b.write(txt)
                print(_('Backup file created : %s.bak') % fname)

        if (txt.find('--catalog=mill') > 0) or (txt.find('-cmill') > 0) :
            ctlog = 'mill'
        elif (txt.find('--catalog=lathe') > 0) or (txt.find('-clathe') > 0) :
            ctlog = 'lathe'
        elif (txt.find('--catalog=plasma') > 0) or (txt.find('-cplasma') > 0) :
            ctlog = 'plasma'

        txt1 = ''
        txt2 = txt.split('\n')
        for line in txt2 :
            txt1 += line.lstrip(' \t') + '\n'

        parser = ConfigParser.RawConfigParser()
        try :
            parser.readfp(io.BytesIO(txt1))

            #dp = parser.get('DISPLAY', 'DISPLAY').lower()
            #if dp not in ['gmoccapy', 'axis', 'gscreen', 'qtdragon'] :
            #    mess_dlg(_("DISPLAY can only be 'axis', 'gmoccapy' or 'gscreen'"))
            #    sys.exit(-1)

            try :
                old_sub_path = ':' + parser.get('RS274NGC', 'SUBROUTINE_PATH')
            except :
                old_sub_path = ''

            try :
                c = parser.get('DISPLAY', 'LATHE')
                if c.lower() in ['1', 'true'] :
                    ctlog = 'lathe'
            except :
                pass

            txt = re.sub(r"%s" % req, '', txt)

            if dp == 'axis' :
                if in_tab :
                    newstr = '%s%s%s%s %s\n' % (req, 'EMBED_TAB_NAME = NativeCAM\n', \
                            'EMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=', \
                            ctlog, path2ui)
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)
                else :
                    newstr = '%sGLADEVCP = -U --catalog=%s %s\n' % (req, ctlog, path2ui)
                    try :
                        oldstr = 'GLADEVCP = %s' % parser.get('DISPLAY', 'gladevcp')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            elif (dp == 'gmoccapy') :
                if in_tab :
                    newstr = '%s%s%s%s%s %s\n' % (req, 'EMBED_TAB_NAME = NativeCAM\n', \
                            'EMBED_TAB_LOCATION = ntb_user_tabs\n', \
                            'EMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=', \
                            ctlog, path2ui)
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)
                else :
                    newstr = '%sEMBED_TAB_LOCATION = box_right\n' % req
                    try :
                        oldstr = 'EMBED_TAB_LOCATION = %s' % parser.get('DISPLAY', 'embed_tab_location')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                    newstr = '%sEMBED_TAB_NAME = right_side_panel\n' % req
                    try :
                        oldstr = 'EMBED_TAB_NAME = %s' % parser.get('DISPLAY', 'embed_tab_name')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                    newstr = '%sEMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=%s %s\n' % (req, ctlog, path2ui)
                    try :
                        oldstr = 'EMBED_TAB_COMMAND = %s' % parser.get('DISPLAY', 'embed_tab_command')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            else :  # gscreen
                newstr = '%sEMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=%s %s\n' % (req, ctlog, path2ui)
                try :
                    oldstr = 'EMBED_TAB_COMMAND = %s' % parser.get('DISPLAY', 'embed_tab_command')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                newstr = '%sEMBED_TAB_LOCATION = vcp_box\n' % req
                try :
                    oldstr = 'EMBED_TAB_LOCATION = %s' % parser.get('DISPLAY', 'embed_tab_location')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                newstr = '%sEMBED_TAB_NAME = NativeCAM\n' % req
                try :
                    oldstr = 'EMBED_TAB_NAME = %s' % parser.get('DISPLAY', 'embed_tab_name')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            newstr = '%sPROGRAM_PREFIX = ncam/scripts/\n' % req
            try :
                oldstr = 'PROGRAM_PREFIX = ' + parser.get('DISPLAY', 'program_prefix')
                txt = re.sub(r"%s" % oldstr, newstr, txt)
            except :
                txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            newstr = '%sNCAM_DIR = ncam\n' % req
            try :
                oldstr = 'NCAM_DIR = ' + parser.get('DISPLAY', 'ncam_dir')
                txt = re.sub(r"%s" % oldstr, newstr, txt)
            except :
                txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            if not 'ncam/my-stuff:ncam/lib/' in old_sub_path :
                newstr = '%sSUBROUTINE_PATH = ncam/my-stuff:ncam/lib/%s:ncam/lib/utilities%s\n' % \
                    (req, ctlog, old_sub_path)
                try :
                    oldstr = 'SUBROUTINE_PATH = ' + parser.get('RS274NGC', 'subroutine_path')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[RS274NGC\]", "[RS274NGC]\n" + newstr, txt)

            open(fname, 'w').write(txt)

            with open(fname, 'w') as b :
                b.write(txt)
                print(_('Success in modifying inifile :\n  %s') % fname)

        except Exception as detail :
            self.err_exit(_('Error modifying ini file\n%(err_details)s') % {'err_details':detail})

def usage():
    print("""
Standalone Usage:
   ncam [Options]

Options :
    -h | --help                this text
   (-i | --ini=) inifilename   inifile used
   (-c | --catalog=) catalog   valid catalogs = mill, plasma, lathe
    -t | --tab                 axis and gmoccapy only, put NativeCAM in a new tab

To prepare your inifile to use NativeCAM embedded,
   a) Start in a working directory with your LinuxCNC configuration ini file
   b) Type this command :
     ncam (-i | --ini=)inifilename (-c | --catalog=)(valid catalog for this configuration)

   A backup of your inifile will be created before it is modified.

   After success, you can use it embedded  :
     linuxcnc inifilename

""")


if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *


    # process args
    args = sys.argv[1:]
    if "-h" in args or "--help" in args:
        usage()
        sys.exit(0)

    try :
        optlist, args = getopt.getopt(sys.argv[1:], 'c:i:t', ["catalog=", "ini="])
    except getopt.GetoptError as err:
        print(err)  # will print something like "option -a not recognized"
        usage()
        sys.exit(2)

    optlist = dict(optlist)

    if "-i" in optlist :
        ini = optlist["-i"]
    elif "--ini" in optlist :
        ini = optlist["--ini"]
    else :
        ini = None

    if (ini is not None) :
        if "-c" in optlist :
            catalog = optlist["-c"]
        elif "--catalog" in optlist :
            catalog = optlist["--catalog"]
        else :
            catalog = DEFAULT_CATALOG
        if not catalog in VALID_CATALOGS :
            usage()
            sys.exit(3)

        in_tab = ("-t" in optlist) or ("--tab" in optlist)
        verify_ini(os.path.abspath(ini), catalog, in_tab)

    app = QApplication(sys.argv)

    ncam = NCamWindow()
    ncam.show()
    sys.exit( app.exec_() )

