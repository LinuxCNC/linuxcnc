from PyQt5.QtWidgets import QMainWindow, QAction,\
         QToolBar, QMessageBox,QTreeWidget, QTreeWidgetItem
from PyQt5 import uic
from PyQt5.QtCore import QObject, QTimer
from PyQt5.QtCore import Qt,QSize
from PyQt5.QtGui import QColor, QIcon, QFont, QImage
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *

import os

import gcode

from message import Message
from qt5_graphics import Lcnc_3dGraphics
from qtvcp.core import Info

INFO = Info()
############################################################

GENERATED_FILE = "ncam.ngc" #TODO from qtnccam file, Does it change?
SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int', 'gc-lines',
                        'tool', 'gcode', 'text', 'list', 'float', 'string', 'engrave',
                        'combo', 'combo-user', 'items', 'filename', 'prjname']

HORIZONTAL_HEADERS = ("Property", "Value")
tick = QImage('graphics/check.png')

class tv_select :  # 'enum' items
    none, feature, items, header, param = list(range(5))

class MetaClass(object):
    '''
    a trivial custom data object
    '''
    def __init__(self, xml, tool_tip, m_visible, is_visible, tool_list = None, options = None):
        self.xml = xml
        self.tool_tip = tool_tip
        self.m_visible = m_visible
        self.is_visible = is_visible
        self.tool_list = tool_list
        self.options = options

    def find_attr(self, name):
        try:
            attr = self.xml.attr[name] if name in self.xml.attr else ''
            return attr
        except:
            return None

    def set_attr(self, name,value):
        try:
            self.xml.attr[name] = value
            return True
        except Exception as e:
            print(e)
            return False

    # call xml (Feature or Parameter) class methods if available
    def __getattr__(self, attr):
        return getattr(self.xml, attr)

    def __repr__(self):
        return "meta Object - %s %s %s"% (self.xml, self.tool_tip, self.m_visible)
    
class TreeItem(object):
    '''
    a python object used to return row/column data, and keep note of
    it's parents and/or children
    '''
    def __init__(self, parentItem, data, header = 'test' ):
        self.meta = data
        self.parentItem = parentItem
        self.header = header
        self.childItems = []

    def appendChild(self, item):
        self.childItems.append(item)

    def insertChild(self,row, item):
        print('insert',self.childItems,row, len(self.childItems))
        if row > len(self.childItems):
            return False
        if row <0:
            return False

        ret = self.childItems.insert(row,item)
        print(self.childItems, ret)
        return ret

    def child(self, row):
        try:
            return self.childItems[row]
        except:
            return None

    def childCount(self):
        return len(self.childItems)

    def columnCount(self):
        return 2
    
    def data(self, column):
        if self.meta == None:
            if column == 0:
                return QVariant(self.header)
            elif column == 1:
                return QVariant("")                
        else:
            if column == 0:
                try:
                    name = self.meta.get_name()
                    #print('error:',self.meta)
                except Exception as e:
                    print(e,'\nerror:',self.meta)
                    #name = self.meta.xml.attr['name'].lower() if 'name' in self.meta.xml.attr else ''
                    return None
                return QVariant(name)
                #return QVariant(self.meta.tool_tip)
            if column == 1:
                value = self.meta.get_value()
                return QVariant(value)
        return QVariant()

    def delete(self):
        self.parentItem.childItems.remove(self)

    def parent(self):
        return self.parentItem

    def atTop(self):
        print(self.parentItem)
        return bool(self.parentItem == None)

    def nextSibling(self):
        next = self.row() +1
        if next > self.parentItem.childCount()-1:
            return None
        return self.parentItem.child(next)

    def row(self):
        if self.parentItem:
            return self.parentItem.childItems.index(self)
        return 0

    def canMoveUp(self):
        print('up?',self.parentItem.childItems.index(self))
        if self.parentItem.childItems.index(self) == 0:
            return False
        return True

    def canMoveDown(self):
        print('down?',self.parentItem.childItems.index(self) )
        if self.parentItem.childItems.index(self) == childCount()-1:
            return False
        return True

    def findPath(self):
        def look(i,path):
                #print('look item',i, path,i.parentItem )
                if not i.parentItem.meta.find_attr('name') == None:
                    look(i.parentItem, path + str(i.row()))
                return path
        #print('look done',look(self, str(self.row())))
        return look(self, str(self.row()))

    def getFeature(self):
        return self.meta.xml

    def __iter__(self):
        self._iternum = -1
        return self

    def __next__(self):
        self._iternum += 1
        if self._iternum < self.childCount():
            #print('next:',self.child(self._iternum),self._iternum,self.childCount(),self.childItems)
            return self.child(self._iternum)
        else:
            raise StopIteration

    def __repr__(self):
        return "Item Object - %s (%s children)"% (self.meta.find_attr('name'), self.childCount())

class treeModel(QAbstractItemModel):
    '''
    a model to display a few names, ordered by sex
    '''
    def __init__(self, parent=None):
        super(treeModel, self).__init__(parent)
        self.rootItem = TreeItem(None, MetaClass(None, 'Root Item', False, False), "ROOT")
        self.parents = {None:self.rootItem}

    def clear(self):
        self.beginResetModel()
        self.rootItem = TreeItem(None, MetaClass(None, 'Root Item', False, False), "ROOT")
        self.parents = {None:self.rootItem}
        self.endResetModel()

    def columnCount(self, parent=None):
        if parent and parent.isValid():
            return parent.internalPointer().columnCount()
        else:
            return len(HORIZONTAL_HEADERS)

    # get data from the treemodel
    def data(self, index, role):
        #print('role',index, role)
        if not index.isValid():
            print('not valid')
            return QVariant()
        item = index.internalPointer()

        if role == Qt.DisplayRole:

            if item.meta and index.column() == 1:

                # don't display data if there is a checkbox
                if item.meta.get_type() == 'bool':
                    return None

                elif item.meta.get_type() == 'combo':
                    def get_data(): # from XML object
                        return item.data(index.column()).value()
                    optionList = item.meta.get_options().split(':')
                    #print('option list',optionList, get_data())
                    for temp in(optionList):
                        text,num = temp.split('=')
                        #print ('look at ',text,num)
                        if str(num) == str(get_data()):
                            #print('bingo')
                            return text
                    return('Unknown {}'.format(get_data()))
                elif item.meta.get_type() == 'tool':
                    table = item.meta.tool_list
                    print(table.list)

            return item.data(index.column())

        elif role == Qt.FontRole:
            if item.childCount() == 0:
                return QFont("Times New Roman", pointSize = 15)
            else:
                return QFont("Times New Roman", pointSize = 15, weight=QFont.Bold)

        elif role == Qt.BackgroundRole and index.column() == 0:
            if item.childCount() > 0:
                return QColor(225, 225, 225)

        elif role == Qt.SizeHintRole:
            return QSize(32,32)

        elif role == Qt.UserRole:
            if item:
                return item.meta

        elif role == Qt.ToolTipRole:
            return item.meta.get_tooltip()

        elif role == Qt.DecorationRole and item.meta and index.column() == 0:
            value =  item.meta.get_icon(16)
            return QIcon('graphics/{}'.format(value))

        elif role == Qt.CheckStateRole and item.meta and index.column() == 1:
            if item.meta.find_attr('type') == 'bool':
                if item.meta.find_attr('value') == '1':
                    return Qt.Checked
                else:
                    return Qt.Unchecked

        return None

    def setData(self, index, value, role):
        col = index.column()
        row = index.row()

        if not index.isValid():
            return False
        item = index.internalPointer()

        print('model setData:',value)
        print('=>', item.meta,role)

        if role == Qt.CheckStateRole:
          if item.meta.find_attr('type') in ('bool'):
            if value == Qt.Checked:
                item.meta.set_value('1')
            else:
                print(item.meta.set_value)
                item.meta.set_value('0')
            self.dataChanged.emit(index, index)
            return True

        elif role == Qt.EditRole:
            print('display',role,index.column())
            if item.meta and index.column() == 1:
                #print(item.meta.find_attr('type'))
                if item.meta.get_type()  in ('float,''int'):
                    print('int/float',item.meta.xml)
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)
                elif item.meta.get_type() == 'tool':
                    print('Tool',value)
                    #dval = TOOL_TABLE.get_text(val)
                else:
                    print('unknown:',item.meta.find_attr('type'))

        elif role == Qt.DisplayRole:
            if item.meta and index.column() == 1:
                if item.meta.get_type() in ('combo', 'combo-user', 'list'):
                    optionList = item.meta.get_options().split(':')
                    #print(optionList,value)
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)

        print('after=>', item.meta)
        #item.meta.fname = value

        return True

    def get_feature(self,item, column=0):
        if column == 0:
            return item.meta.xml

    # Returns the item flags for the given index.
    def flags(self, index):
        if not index.isValid():
            return None
        if index.column() == 1:
            return Qt.ItemIsEnabled | Qt.ItemIsUserCheckable | Qt.ItemIsSelectable | Qt.ItemIsEditable
        else:
            return Qt.ItemIsEnabled | Qt.ItemIsSelectable

    def headerData(self, column, orientation, role):
        if (orientation == Qt.Horizontal and
        role == Qt.DisplayRole):
            try:
                return QVariant(HORIZONTAL_HEADERS[column])
            except IndexError:
                pass

        return None

    def index(self, row, column, parent):
        if not self.hasIndex(row, column, parent):
            return QModelIndex()

        if not parent.isValid():
            parentItem = self.rootItem
        else:
            parentItem = parent.internalPointer()

        childItem = parentItem.child(row)
        if childItem:
            return self.createIndex(row, column, childItem)
        else:
            return QModelIndex()

    def indexToPath(self, index):
        # creates a gtk style path (0:1:2) from index

        p = (index.parent().parent().row(),index.parent().row(),index.row())
        path =''
        for num,i in enumerate(p):
            if i == -1:
                continue
            path += str(i)
            if num <2:
                path += ':'
        if path =='':
            return None
        return path

    def parent(self, index):
        if not index.isValid():
            return QModelIndex()

        childItem = index.internalPointer()
        if not childItem:
            return QModelIndex()
        
        parentItem = childItem.parent()

        if parentItem == self.rootItem:
            return QModelIndex()

        if parentItem is None:
            return QModelIndex()
        return self.createIndex(parentItem.row(), 0, parentItem)

    def item_parent(self, item):
        return item.parentItem

    def printItemName(self, item):
        try:
            name = item.meta.find_attr('name')
        except:
            return 'None'
        return name

    def rowCount(self, parent=QModelIndex()):
        if parent.column() > 0:
            return 0
        if not parent.isValid():
            p_Item = self.rootItem
        else:
            p_Item = parent.internalPointer()
        return p_Item.childCount()

    def removeRows(self, row, count, parentIndex):
        self.beginRemoveRows(parentIndex,row,row+count)

        parentItem = self.getItemFromIndex(parentIndex)
        for i in range(row, row+count):
            print('remove',i,parentItem.child(i))
            parentItem.child(i).delete()

        self.endRemoveRows()

    def insertItem(self, row, parentItem, item):
        print('insert item',row, item)
        parentItem.insertChild(row, item)
        self.layoutChanged.emit()

    def moveRow(self,item, delta):

        parent = item.parent()
        parentIndex = self.createIndex(parent.row(), 0, parent)
        row = item.row()

        self.removeRows(row,1,parentIndex)
        self.insertItem(row+delta, parent, item )

        return
        # in almost all QTreeView implementations the "tree" structure is at column 0...
        curr_index = self.selectionModel().currentIndex().siblingAtColumn(0)
        if curr_index.isValid():
            curr_item = self.model().itemFromIndex(curr_index)
            curr_row = curr_index.row()
            # if this is the top sibling of its parent it cant be moved up... 
            if curr_row > 0: 
                parent_item = self.model().itemFromIndex(curr_index.parent())
                # NB parent_item is None in the case of root children: 
                # in that case you therefore use "takeRow" and "insertRow" methods of the model, not the item!
                take_row = self.model().takeRow if parent_item == None else parent_item.takeRow 
                insert_row = self.model().insertRow if parent_item == None else parent_item.insertRow 
                row_to_move = take_row(curr_row)
                insert_row(curr_row - 1, row_to_move)
                new_index = self.model().indexFromItem(curr_item)
                # now set the (single) selection, and set current,  back to the moved item 
                # (if you are implementing single-selection, obviously)
                flag = QtCore.QItemSelectionModel.SelectionFlag
                self.selectionModel().setCurrentIndex(new_index, flag.Clear | flag.SelectCurrent)

    def addNode(self, parent, data):
            xml, tool_tip, m_visible, is_visible, toollist = data
            if parent is None:
                parentItem = self.rootItem
            else:
                parentItem = parent
            #print('addNode',parentItem)
            temp = MetaClass(xml, tool_tip, m_visible, is_visible, toollist)
            newItem = TreeItem(parentItem, temp)
            #print('add item',self. printItemName(newItem),'to',self. printItemName(parentItem),parentItem)

            self.layoutAboutToBeChanged.emit()
            parentItem.appendChild(newItem)
            self.layoutChanged.emit()
            return newItem

    def searchModel(self, meta):
        '''
        get the modelIndex for a given appointment
        '''
        def searchNode(node):
            '''
            a function called recursively, looking at all nodes beneath node
            '''
            for child in node.childItems:
                if meta == child.meta:
                    index = self.createIndex(child.row(), 0, child)
                    return index
                    
                if child.childCount() > 0:
                    result = searchNode(child)
                    if result:
                        return result
        
        retarg = searchNode(self.parents[0])
        print (retarg)
        return retarg
            
    def find_GivenName(self, fname):
        app = None
        for meta in self.people:
            if meta.fname == fname:
                app = meta
                break
        if app != None:
            index = self.searchModel(app)
            return (True, index)            
        return (False, None)

    # get the top item
    def get_iter_root(self):
        return self.rootItem

    def getFirstItem(self):
        return self.rootItem.child(0)

    def getFirstChildItem(self, parent):
        return parent.child(0)

    def getNextSibling(self, item):
        '''
        Return the next sibling of the item or None
        '''
        return item.nextSibling()

    def getItemFromIndex(self,index, column = 0):

        if isinstance(index, QModelIndex):
            return index.internalPointer()

        # from list of model indexes:
        if not index == []:
            item = index[column].internalPointer()
            print ('selected: row',index[column].row(), index[column].internalPointer()) 
            return item
        return None

    def forEachParent(self,function):
        for child in self.rootItem.childItems:
            function(child)

    def remove(self, item):
        print('REMOVE!: ',item)
        item.delete()
        self.layoutChanged.emit()

class ComboDelegate(QStyledItemDelegate):
    """
    A delegate that places a fully functioning QComboBox in every
    cell of the column to which it's applied
    """
    def __init__(self, parent):
        super(ComboDelegate, self).__init__(parent)

    @staticmethod
    def data(index):
        """
        Custom method to derive the model data from the model.
         
        Note: assumes there is an intermediate proxyModel.
        """
        # get the sourceModel, we know that the index.model() is a proxy model. 
        # If you don't know you can use the following:
        proxy_model = index.model()
        try:
             source_model = proxy_model.sourceModel()
             source_index = proxy_model.mapToSource(index)
        except:
             source_model = proxy_model
             source_index = index
        return source_model.getItemFromIndex(source_index).qt_data(index.column())

    def createEditor(self, parent, option, index):
        if index.data() =='': return
        isComboType = bool(index.model().data(index,Qt.UserRole).get_type() == 'combo')
        if isComboType:
            combo = QComboBox(parent)
            combo.setFont(QFont("Times New Roman", pointSize = 15, weight=QFont.Bold))

            # add text and user data from XML'options' to combobox
            items = index.model().data(index,Qt.UserRole).get_options().split(':')
            for i in items:
                data = i.split('=') # text, user data ('magic number')
                combo.addItem(data[0],data[1])
            combo.currentIndexChanged.connect(self.currentIndexChanged)
            return combo

        else:
            return super().createEditor(parent, option, index)

    # set combobox display
    def setEditorData(self, editor, index):
        isComboType = bool(index.model().data(index,Qt.UserRole).get_type() == 'combo')
        # set combo box index from treemodel
        if isComboType:
            # get model data 'magic number'
            value = index.model().data(index,Qt.UserRole).get_value()
            idx = editor.findData(value) 
            #print('set combo', idx)
            editor.setCurrentIndex(value)
            return
        else:
            return super().setEditorData(editor, index)

    def setModelData(self, editor, model, index):
        isComboType = bool(index.model().data(index,Qt.UserRole).get_type() == 'combo')
        # set model to combobox selection
        if isComboType:
            if index.column() == 1:
                # extract integer user data ('magic number') from combobox
                data = editor.itemData(editor.currentIndex(), Qt.UserRole)
                #print('set model data',data)
                model.setData(index, data, Qt.DisplayRole)
        else:
            return super().setModelData(editor, model, index)

    @pyqtSlot()
    def currentIndexChanged(self):
        self.commitData.emit(self.sender())
        self.closeEditor.emit(self.sender())
########
# custom widgets
###########
class ToolButton(QWidget):
    def __init__(self, text, path_icon, tooltip, src, parent=None):
        super(ToolButton, self).__init__(parent)
        lay = QVBoxLayout(self)
        toolButton = QToolButton()
        if path_icon is not None :
            toolButton.setIcon(QIcon('graphics/{}'.format(path_icon)))
            toolButton.setIconSize(QSize(64, 64))
        if (tooltip is not None) and (tooltip != '') :
            toolbutton.setToolTip(tooltip)
        if src is not None :
            print('add click')
            toolbutton.clicked.connect(lambda s, src=src: self.add_feature(None, src))

        label = QLabel(text)
        lay.addWidget(toolButton, 0, Qt.AlignCenter)
        lay.addWidget(label, 0, Qt.AlignCenter)
        lay.setContentsMargins(0, 0, 0, 0)


###############
# Window
###############
class NCamWindow(QMainWindow):

    def __init__(self, parent=None):
        super(NCamWindow, self).__init__(parent)
        try:
            self.filename = './ncam.ui'
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

        self._model = treeModel()
        self._model.dataChanged.connect(lambda :self.update_do_btns(True))
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
        self.zMessanger = Message()
 
############
# Gcode Display
############

    def addDisplayWidget(self):
        self.emptypath = os.path.join( "/tmp/LINUXCNCtempEmpty.ngc")

        emptyfile = open(self.emptypath, "w")
        print(("m2"), file=emptyfile)
        emptyfile.close()

        self.graphics = Lcnc_3dGraphics()
        self.graphics.setINI('/home/chris/Downloads/NativeCAM-master/configs/sim/axis/ncam_demo/mill-mm.ini')
        self.setGraphicsDisplay()

        self.loadDisplay(self.emptypath)

        self.displayLayout.addWidget(self.graphics)

    def setGraphicsDisplay(self):
        # class patch to catch gcode errors - in theory
        self.graphics.report_gcode_error = self.report_gcode_error
        # reset traverse color so other displays don't change
        self.defaultColor = self.graphics.colors['traverse']
        self.graphics.current_view = 'z'
        self.graphics.metric_units = INFO.MACHINE_IS_METRIC
        self.graphics.use_gradient_background = True
        self.graphics.show_tool = False
        self.graphics.grid_size = 2
        self.graphics.cancel_rotate = True

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
                            icon = QIcon('graphics/{}'.format(iconName))

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
        ca(self.actionLoadCfg,"LoadCfg", 'gtk.STOCK_OPEN', _('Add a Prototype Subroutine'), '', _('Add a Subroutine Definition File'), self.action_loadCfg)
        ca(self.actionImportXML,"ImportXML", 'gtk.STOCK_REVERT_TO_SAVED', _('Import a Project File'), None, _('Import a Project Into the Current One'), self.action_importXML)

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
                icon = QIcon('graphics/{}'.format(items[3]))
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
        self.iconGroupBox.setTitle('Add Icons')
        hlay = self.iconGridLayout
        self.add_catalog_items(self.iconGridLayout)
        hlay.setContentsMargins(5, 5, 5, 5)

    def add_catalog_items(self, menu_add):

        def add_to_menu(grp_menu, path) :
            for ptr in range(len(path)) :
                try :
                    p = path[ptr]
                    if p.tag.lower() in ["menu", "menuitem", "group", "sub"] :
                        name = p.get("name") if "name" in p.keys() else ""
                        icon = p.get('icon')
                        tooltip = _(p.get("tool_tip")) if "tool_tip" in p.keys() else None
                        src = p.get('src')

                        # add toolbutton to groupbox
                        try:
                            btn = ToolButton(name, icon, _(tooltip), src)
                            self.iconGridLayout.addWidget(btn)
                        except:
                            pass

                        if p.tag.lower() in ['menu', "group"] :
                            sub = self.actionMenuAdd.addMenu(QIcon('graphics/{}'.format(icon)), name)
                            add_to_menu(sub, p)
                        else:
                            act = grp_menu.addAction(QIcon('graphics/{}'.format(icon)),name)
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
        dlg.setStandardButtons(QMessageBox.Ok)
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
        dlg.setText(mess)
        dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        dlg.setIcon(QMessageBox.Question)
        dlg.show()
        forceDetailsOpen(dlg)

        button = dlg.exec()

        if button == QMessageBox.Yes:
            return True
        else:
            return False

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

