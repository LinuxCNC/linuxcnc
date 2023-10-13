// Dialog file open library, https://github.com/samhocevar/portable-file-dialogs/blob/master/examples/example.cpp
#include <portable-file-dialogs.h>
#if _WIN32
#define DEFAULT_PATH "C:\\"
#else
#define DEFAULT_PATH "/tmp"
#endif

void MainWindow::on_toolButton_open_dxf_pressed(){
    if (!pfd::settings::available()){
        std::cout << "Portable File Dialogs are not available on this platform. \n"
                     "On linux install zenity, $ sudo apt-get install zenity\n";
    }
    auto f = pfd::open_file("Choose files to read", DEFAULT_PATH,
                            { "Dxf Files (.dxf)", "*.dxf",
                              "All Files", "*" }, pfd::opt::none); // Or ::multiselect.
    open_dxf_file(f.result().at(0)); // This lib can open multiple results.
}
