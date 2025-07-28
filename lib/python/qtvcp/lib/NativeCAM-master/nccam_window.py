import os

from PyQt5.QtWidgets import QMainWindow, QAction,\
         QToolBar, QMessageBox,QTreeWidget, QTreeWidgetItem
from PyQt5 import uic
from PyQt5.QtCore import QObject, QTimer
from PyQt5.QtCore import Qt,QSize
from PyQt5.QtGui import QColor, QIcon, QFont, QImage
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *

import gcode
# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)
LOG.error('lOGGING WORKS')

from message import ZMQMessage
from custom_widgets import MetaClass, TreeItem, tv_select, ComboDelegate, treeModel, ToolButton, IconView
from qt5_graphics import Lcnc_3dGraphics
from qtvcp.core import Info, Status

INFO = Info()
STATUS = Status()
############################################################

GENERATED_FILE = "ncam.ngc" #TODO from qtnccam file, Does it change?
SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int', 'gc-lines',
                        'tool', 'gcode', 'text', 'list', 'float', 'string', 'engrave',
                        'combo', 'combo-user', 'items', 'filename', 'prjname']

###############
# Window
###############
class NCamWindow(QMainWindow):

    def __init__(self, parent=None):
        super(NCamWindow, self).__init__(parent)
        try:
            current_dir =  os.path.dirname(__file__)
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

        # for reloads in qtvcp screens
        self.zMessanger = ZMQMessage()
 
############
# Gcode Display
############

    def addDisplayWidget(self):
        self.emptypath = os.path.join( "/tmp/LINUXCNCtempEmpty.ngc")

        emptyfile = open(self.emptypath, "w")
        print(("m2"), file=emptyfile)
        emptyfile.close()

        self.graphics = Lcnc_3dGraphics(inipath=STATUS.stat.ini_filename)
        self.setGraphicsDisplay()

        self.loadDisplay(self.emptypath)

        self.displayLayout.addWidget(self.graphics)

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
        print(errortext)

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
        print(('m2'), file=self.workfile)

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
                    print('callback',arg)
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
        ca(self.actionSaveNGC,"SaveNGC", None, _('Export gcode as RS274NGC'), '', _('Export gcode as RS274NGC'), self.action_save_ngc)
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
        ca(self.actionAppendItm,"AppendItm", 'gtk.STOCK_INDENT', _("Add to Items"), "<control>Right", _("Add to Items"), self.action_appendItm)
        ca(self.actionRemoveItm,"RemoveItm", 'gtk.STOCK_UNINDENT', _("Remove from Items"), "<control>Left", _('Remove from Items'), self.action_removeItem)
        ca(self.actionMoveUp,"MoveUp", QStyle.SP_ArrowUp, _('Move up'), "<control>Up", _('Move up'), self.move, -1)
        ca(self.actionMoveDown,"MoveDown", QStyle.SP_ArrowDown, _('Move down'), "<control>Down", _('Move down'), self.move, 1)
        ca(self.actionSaveUser,"SaveUser", QStyle.SP_DialogSaveButton, _('Save Values as Defaults'), '', _('Save Values of this Subroutine as Defaults'), self.action_saveUser)
        ca(self.actionDeleteUser,"DeleteUser", QStyle.SP_TitleBarCloseButton, _("Delete Custom Default Values"), None, _("Delete Custom Default Values"), self.action_deleteUser)
        #ca(self.actionSetDigits,"SetDigits", None, _('Set Digits'), None, None, None)
        #ca(self.actionDigit1,"Digit1", None, '1', None, None, self.action_digits, '1')
        #ca(self.actionDigit2,"Digit2", None, '2', None, None, self.action_digits, '2')
        #ca(self.actionDigit3,"Digit3", None, '3', None, None, self.action_digits, '3')
        #ca(self.actionDigit4,"Digit4", None, '4', None, None, self.action_digits, '4')
        #ca(self.actionDigit5,"Digit5", None, "5", None, None, self.action_digits, '5')
        #ca(self.actionDigit6,"Digit6", None, '6', None, None, self.action_digits, '6')

        # actions related to adding subroutines
        #ca(self.actionAddMenu,"AddMenu", None, _("_Add"), None, None, None)
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
        ca(self.actionCurrent,"Current", 'gtk.STOCK_SAVE', _("Save Project as Current Work"), '', _('Save Project as Current Work'), self.action_saveCurrent)
        ca(self.actionBuild,"Build", 'build.png', _('Generate %(filename)s') % {'filename':GENERATED_FILE}, None,
                     _('Generate %(filename)s and load it in LinuxCNC') % {'filename':GENERATED_FILE}, self.action_build)
        ca(self.actionRename,"Rename", None, _("Rename Subroutine"), None, _('Rename Subroutine'), self.action_renameF)
        ca(self.actionChngGrp,"ChngGrp", None, _("Group <-- --> Sub-group"), None, _('Group <-- --> Sub-group'), self.action_chng_group)
        ca(self.actionDataType,"DataType", None, _("Change to GCode"), None, _('Change to GCode'), self.action_gcode)
        ca(self.actionRevertType,"RevertType", None, _("Revert to original type"), None, _('Revert to original type'), self.action_revert_type)

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
        self.iconView = IconView()
        self.iconViewVerticalLayout.addWidget(self.iconView)

        self.iconGroupBox.setTitle('Add Icons')
        hlay = self.iconGridLayout
        self.add_catalog_items(hlay)
        hlay.setContentsMargins(5, 5, 5, 5)

        self._toolbar = QToolBar()
        self._toolbar.addAction(self.actionLoadCfg)
        self._toolbar.addAction(self.actionImportXML)
        self._toolbar.addAction(self.actionBack)

        self.iconViewVerticalLayout.setMenuBar(self._toolbar)
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

                            # add toolbutton to groupbox
                            try:
                                btn = ToolButton(name, iconPath, _(tooltip), src)
                                self.iconGridLayout.addWidget(btn, self._row, self._col)
                                self._col +=1
                                if self._col > 2:
                                    self._col = 0
                                    self._row +=1
                            except:
                                pass
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
                self.items_ts_parent_s = self.treestore.get_string_from_iter(sortedIndex)

                self.items_path = model.get_path(itr)
                n_children = model.iter_n_children(itr)
                self.items_lpath = (self.items_path + (n_children,))

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
                self.items_ts_parent_s = selectedItem.meta.xml.get_name()

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
        self.mess_dlg(_('LinuxCNC needs to be restarted now'))

#############
# dialogs
#############

    def helpDialog(self, **kward):

        lic = kward.get('licence') or ''
        ver = kward.get('version') or ''

        self.mess_dlg(title =  kward.get('title') or 'Help',
                      mess = lic, info = ver, winTitle='About')

    def openDialog(self, **kward):

        dlg = QFileDialog()
        dlg.setFileMode(QFileDialog.ExistingFile)
        dlg.setAcceptMode(QFileDialog.AcceptOpen)
        dlg.setWindowTitle(kward.get('extensions', 'Open Dialog'))
        dlg.setNameFilter(kward.get('extfilter',"All Files (*);;Text Files (*.txt)"))

        if not kward.get('extensions') is None:
            dlg.setNameFilter(kward.get('extensions'))
        if not kward.get('directory') is None:
            dlg.setDirectory(kward.get('directory'))

        # sidebar links
        urls = []
        urls.append(QUrl.fromLocalFile(os.path.expanduser('~')))
        local = os.path.join(os.path.expanduser('~'),'linuxcnc/nc_files')
        if os.path.exists(local):
            urls.append(QUrl.fromLocalFile(local))
        dlg.setSidebarUrls(urls)

        filename = None
        if dlg.exec_():
           filename = dlg.selectedFiles()[0]
           path = dlg.directory().absolutePath()

        return filename

    def action_save_ngc(self, *arg) :
        filechooserdialog = gtk.FileChooserDialog(_("Save as ngc..."), None,
            gtk.FILE_CHOOSER_ACTION_SAVE,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try :
            filt = gtk.FileFilter()
            filt.set_name("NGC")
            filt.add_mime_type("text/ngc")
            filt.add_pattern("*.ngc")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(NGC_DIR)
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                gcode = self.to_gcode()
                filename = filechooserdialog.get_filename()
                if filename[-4] != ".ngc" not in filename :
                    filename += ".ngc"
                with open(filename, "wb") as f:
                    f.write(self.to_gcode())
                f.close()
        finally :
            filechooserdialog.destroy()

    def mess_dlg(self, mess, winTitle = '', title="NativeCAM", info='', icon=QMessageBox.Critical):
        def forceDetailsOpen(dlg):
          try:
            # force the details box open on first time display
            for i in dlg.buttons():
                if dlg.buttonRole(i) == QMessageBox.ActionRole:
                    for j in dlg.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
          except:
            pass

        dlg = QMessageBox()
        dlg.setWindowTitle(winTitle)
        dlg.setTextFormat(Qt.RichText)
        dlg.setText('<b>{}</b>'.format(title))
        dlg.setInformativeText(info)
        dlg.setStandardButtons(QMessageBox.Ok | QMessageBox.Cancel)
        dlg.setIcon(icon)
        dlg.setDetailedText(mess)
        dlg.show()
        forceDetailsOpen(dlg)

        retval = dlg.exec()

    def mess_yesno(self, mess, title = "Nativecam"):
        def forceDetailsOpen(dlg):
          try:
            # force the details box open on first time display
            for i in dlg.buttons():
                if dlg.buttonRole(i) == QMessageBox.ActionRole:
                    for j in dlg.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
          except:
            pass

        dlg = QMessageBox()
        dlg.setTextFormat(Qt.RichText)
        dlg.setText('<b>{}</b>'.format(title))
        dlg.setInformativeText(mess)
        dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        dlg.setIcon(QMessageBox.Question)
        dlg.show()
        forceDetailsOpen(dlg)

        button = dlg.exec()

        if button == QMessageBox.Yes:
            return True
        else:
            return False

    def mes_update_sys(self, mess, title = "Nativecam"):
        NO, YES, CANCEL, REFRESH = list(range(4))
        def forceDetailsOpen(dlg):
          try:
            # force the details box open on first time display
            for i in dlg.buttons():
                if dlg.buttonRole(i) == QMessageBox.ActionRole:
                    for j in dlg.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
          except:
            pass
        dlg = QMessageBox()
        dlg.setTextFormat(Qt.RichText)
        dlg.setText('<b>{}</b>'.format(title))
        #dlg.setText(mess)
        dlg.setInformativeText(mess)
        dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No | QMessageBox.Cancel | QMessageBox.RestoreDefaults)
        dlg.setIcon(QMessageBox.Question)
        dlg.show()
        forceDetailsOpen(dlg)

        button = dlg.exec()

        if button == QMessageBox.No:
            return NO
        elif button == QMessageBox.Yes:
            return YES
        elif button == QMessageBox.Cancel:
            return CANCEL
        elif button == QMessageBox.Apply:
            return REFRESH

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

