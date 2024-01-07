import sys
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *

class Template(QWidget):

    def __init__(self):
        super().__init__()
        dat = { 'A':
                    { 'A':
                        {'1.1': ['1.1.1', '1.1.2'],
                         '1.2': ['1.2.1', '1.2.2']
                         },
                     '2':
                        {'2.1': ['2.1.1','2.1.2']}
                    }
             }
        tw = QTreeWidget()
        tw.itemChanged[QTreeWidgetItem, int].connect(self.get_item)
        grid = QGridLayout(self)
        grid.addWidget(tw)
        self.add(tw, dat)

    def get_item(self, item, column):
        if item.checkState(column) == Qt.Checked:
            print(f'{item.text(column)} was checked')
        else:
            print(f'{item.text(column)} was unchecked')

    def new_item(self, text):
        item = QTreeWidgetItem()
        item.setText(0, text)
        item.setCheckState(0, Qt.Unchecked)
        item.setFlags(Qt.ItemIsUserCheckable | Qt.ItemIsEnabled)
        return item

    def add(self, p, ch):
        for k, v in ch.items():
            item = self.new_item(k)
            if isinstance(p, QTreeWidget):
                p.addTopLevelItem(item)
            else:
                p.addChild(item)

            if isinstance(v, dict):
                self.add(item, v)

            elif isinstance(v, list):
                for txt in v:
                    item.addChild(self.new_item(txt))


if __name__ == '__main__':
    app = QApplication(sys.argv)
    gui = Template()
    gui.show()
    sys.exit(app.exec_())
