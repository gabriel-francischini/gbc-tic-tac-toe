// BEWARE: This program user extensions available only on GNU/Linux!!
//   This program directly depends on GNU's C/C++ library (glibc/c++)

// BEWARE: Those leaks into the global namespace, and can't be contained in a
// namespace because stdlibc++ EXPECTS (completely!) that those should be avai-
// lable in the global scope.
extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pty.h>
#include <signal.h>


    // see: https://stackoverflow.com/questions/5656530
    void* create_shared_memory(size_t size) {
        // Our memory buffer will be readable and writable:
        int protection = PROT_READ | PROT_WRITE;

        // The buffer will be shared (meaning other processes can access it), but
        // anonymous (meaning third-party processes cannot obtain an address for it),
        // so only this process and its children will be able to use it:
        int visibility = MAP_SHARED | MAP_ANONYMOUS;

        // The remaining parameters to `mmap()` are not important for this use case,
        // but the manpage for `mmap` explains their purpose.
        return mmap(NULL, size, protection, visibility, -1, 0);
    }

}

#include <exception>
#include <ios>
#include <iostream>
#include <system_error>
#include <streambuf>
#include <fstream>
#include <ext/stdio_filebuf.h>
#include <functional>
#include <chrono>
#include <string>
#include <regex>
#include <tuple>
#include <algorithm>

// Standin for C's File Descriptors. Think of them of a kind of ID.
using FileDescriptor = int;

// C's FileStreams
namespace libc {
    using FileStream = FILE *;
}

namespace gnu {
    // Akin to std::filebuf (aka std::basic_filebuf<char>)
    using filebuf = __gnu_cxx::stdio_filebuf<char>;
}

class Pipe {
private:
    using OpenPipeError = int;
    constexpr static int NoOpenPipeErrors = 0;

    FileDescriptor _fds[2];
    libc::FileStream _filestreams[2];

    gnu::filebuf _fbs[2];
public:

    Pipe(){
        OpenPipeError error = openpty(&this->_fds[1], &this->_fds[0],
                                      nullptr, nullptr, nullptr);
        if(error != NoOpenPipeErrors)
            throw std::system_error(error, std::system_category(),
                                    "Couldn't open a pipe");


        const_cast<libc::FileStream&>(this->master()) = fdopen(this->fd_master(), "r");
        if(this->master() == nullptr)
            throw std::system_error(errno, std::system_category(),
                                    "Couldn't open a pipe's input C stream");
        this->fb_master() = gnu::filebuf(this->master(), std::ios_base::in);

        const_cast<libc::FileStream&>(this->slave()) = fdopen(this->fd_slave(), "r");
        if(this->master() == nullptr)
            throw std::system_error(errno, std::system_category(),
                                    "Couldn't open a pipe's output C stream");
        this->fb_slave() = gnu::filebuf(this->slave(), std::ios_base::in);

    }

    void close() {
        for(auto& filestream : _filestreams){
            auto status = fclose(filestream);
            decltype(status) CloseErrorOccurred = EOF;

            if(status == CloseErrorOccurred){
                // errno usually expands via macro to (*__errno_location())
                // so we can't access it if it is inside a namespace (libc::)

                throw std::system_error(errno, std::system_category(),
                                        "Couldn't close a pipe");
            }
        }
    }

    ~Pipe() = default;

    // FileDescriptors serves as IDs, so they kind of *never will* be modified.
    // However, since they *will* interact with C functions their constness will
    // often be dropped, although they'll never change their value.
    const FileDescriptor& fd_master() const {return this->_fds[1];}
    const FileDescriptor& fd_slave() const {return this->_fds[0];}

    libc::FileStream const& master() const {return this->_filestreams[1];}
    libc::FileStream const& slave() const {return this->_filestreams[0];}

    gnu::filebuf& fb_master() {return this->_fbs[1];}
    gnu::filebuf& fb_slave() {return this->_fbs[0];}

};

class ProcessIOWrapper {
public:
    // those variable are MUTABLE because of C's weird const-correctness
    // Pipes main-out into sub-stdin
    mutable Pipe main2sub;

    // Pipes sub-stdout into main-in
    mutable Pipe sub2main;

private:
    pid_t child_pid;
    bool* is_child_running;

public:
    ProcessIOWrapper() = delete;

    ProcessIOWrapper(std::function<void ()> child_function) :
        main2sub(Pipe()),
        sub2main(Pipe()),
        child_pid(0),

        // This variable is a pointer to a SHARED MEMORY area because
        // the subprocess has a COPY-ON-WRITE of the main memory. Therefore,
        // in order for the child to flag its termination we have to have
        // some SHARED MEMORY that both processes can see and write to.
        is_child_running(static_cast<bool*>(create_shared_memory(sizeof(bool))))
    {
        *this->is_child_running = true;

        // Flushes EVERYTHING before actually forking a new process,
        // so STDOUT/STDIN aren't in an inconsistent state
        fflush(nullptr);

        this->child_pid = fork();

        if(this->child_pid == static_cast<pid_t>(0)){
            // We are in the child subprocess!
            dup2(sub2main.fd_slave(), STDIN_FILENO);
            dup2(sub2main.fd_slave(), STDOUT_FILENO);
            dup2(sub2main.fd_slave(), STDERR_FILENO);

            child_function();

            sub2main.close();
            main2sub.close();

            // On pseudo terminals there is no need to flush before exiting.
            // fflush(nullptr);

            *this->is_child_running = false;
        } else if (child_pid < static_cast<pid_t>(0)){
            // The fork failed!!
            *this->is_child_running = false;
            throw std::system_error(errno, std::system_category(),
                                    "A fork has failed");
        } else {
            // We are in the parent subprocess :D
            // Conceptually, the parent process IS the MAIN process of the
            // C++ runtime (i.e. the caller which constructed this object)
            return;
        }

    };

    const bool& is_running() const {return *this->is_child_running;}
    const pid_t& pid() const {return this->child_pid;}

    // The following are functions for the PUT (i.e. WRITE, CIN) area

    /**! Writes one character to the output sequence. If the output sequence
       write position is not available (the buffer is full), then calls
       overflow(ch).
    **/
    char sputc(char& ch){
        return static_cast<char>(this->main2sub.fb_master().sputc(ch));
    }

    /**! Writes count characters to the output sequence from the character array
       whose first element is pointed to by s. The characters are written as if
       by repeated calls to sputc(). Writing stops when either count characters
       are written or a call to sputc() would have returned Traits::eof().
    **/
    std::streamsize sputn( const char* s, std::streamsize count){
        return this->main2sub.fb_master().sputn(s, count);
    }




    // The following are functions for the GET (i.e. READ, COUT) area

    //! obtains the number of characters immediately available in the get area
    auto in_avail() const {return this->sub2main.fb_master().in_avail();}

    //! reads one character from the input sequence and advances the sequence
    char sbumpc(){
        return static_cast<char>(this->sub2main.fb_master().sbumpc());
    }

    /**! reads one character from the input sequence without advancing the
       sequence
    **/
    char sgetc(){
        return static_cast<char>(this->sub2main.fb_master().sgetc());
    }

    /**! Reads count characters from the input sequence and stores them into a
       character array pointed to by s. The characters are read as if by
       repeated calls to sbumpc(). That is, if less than count characters are
       immediately available, the function calls uflow() to provide more until
       Traits::eof() is returned.
    **/
    std::streamsize sgetn(char* s, std::streamsize count){
        return this->sub2main.fb_master().sgetn(s, count);
    }




    ~ProcessIOWrapper(){
        *this->is_child_running = false;

        // Removes the shared memory at is_child_running
        munmap(this->is_child_running, sizeof(bool));
    }
};

namespace chrono = std::chrono;


// Some hex manipulating primitives.
// see: https://stackoverflow.com/questions/13490977/convert-hex-stdstring-to-unsigned-char

inline int char2hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::runtime_error("wrong char");
}

std::vector<unsigned char> str2hex(const std::string& hexStr) {
    std::vector<unsigned char> retVal;
    bool highPart = ((hexStr.length() % 2) == 0);
    // for odd number of characters - we add an extra 0 for the first one:
    if (!highPart)
        retVal.push_back(0);
    std::for_each(hexStr.begin(), hexStr.end(),
                  [&](char nextChar) {
                      if (highPart)
                          // this is first char for the given hex number:
                          retVal.push_back(0x10 * char2hex(nextChar));
                      else
                          // this is the second char for the given hex number
                          retVal.back() += char2hex(nextChar);
                      highPart = !highPart;
                  }
                  );

    return retVal;
}


class Debugger {
    // ALWAYS REMEMBER: The child process can't write the parent's memory,
    // at best it can only read. It's has a COPY-ON-WRITE scheme.
    constexpr static auto child = [](){
        // printf("This is the subprocess\n speaking to the main process! Or\n is it?");
        // printf(" More text from\n the sub, bro.\n");

        execlp("sameboy", "sameboy", "main.gb", nullptr);
    };

public:
    ProcessIOWrapper pIOW;
    bool debug;

    // Registers of the Game Boy Color
    enum GBReg {
        A=0, F,
        B, C,
        H, L,
        SP0, SP1,
        PC0, PC1,
        GBRegNUM
    };

    unsigned char registers[GBRegNUM];
    unsigned short mem_watch;
    std::vector<std::string> register_log;
    std::vector<std::string> mem_log;
    std::vector<std::string> code_log;

    // It's called Write so it doesn't clashes with C's write()
    void Write(std::string text){
        write(this->pIOW.sub2main.fd_master(), text.c_str(), text.length());
    }

    const bool should_refresh() const {
        return (pIOW.in_avail() > 0);
    }

    void process_input(){
        bool new_line = false;

        if(this->should_refresh()){
            if(pIOW.in_avail() > 0){
                for(unsigned int i=0; i<pIOW.in_avail(); i++){
                    char ch = pIOW.sbumpc();

                    if(ch == '\n'){
                        new_line = true;
                        break;
                    } else if(ch != '\r'){
                        this->line.push_back(ch);
                    }
                }
            }

            if(new_line){
                // Fires up the debugger as soon as the SameBoy starts
                if(!this->has_activated_debugger && std::regex_search(line, std::regex(R"(^AF = \$[0-9A-F]+)"))){
                    if(this->debug)
                        std::cout << "We are gonna TRAP this!" << std::endl;

                    kill(pIOW.pid(), 2);
                    this->has_activated_debugger = true;
                }

                for(const auto& [regex_str, then_part] : this->line_regexes){
                    if(std::regex_search(line, std::regex(regex_str)))
                        then_part(line);
                }

                if(this->recording_log){
                    this->temp_log.push_back(std::string(line));
                }

                if(this->debug && !std::regex_search(line, std::regex(R"(^[0-9a-fA-F]+\:)"))){
                    std::cout << "number of chars: " << line.length()
                              << ", chars: \"" << line.c_str();
                    std::cout << "\"" << std::endl;
                }
            }

            // Clears new_line so next time we scan the terminal everything is
            // alright
            if(new_line){
                this->line.clear();
                new_line = false;
            }

            if(this->should_refresh()){
                // Still things to process!
                this->process_input();
            }
        }
    }

    std::string at_addr(std::string hex_addr){
        auto result = result_of("examine/1 $" + hex_addr + "\n",
                                this->mem_regex_str);

        if (result.size() < 1)
            throw std::runtime_error("result_of examine returned <1 lines");

        std::string s = result[0];

        // Trims the '$' character (in place)
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                    return !(ch == '$');
                }));

        return s.substr(6, 2);
    }

    void refresh(){
        register_log = result_of("registers\n", reg_regex_str);
        process_input();

        bool deb = this->debug;
        this->debug = false;
        mem_log = result_of("examine/1024 " + std::to_string(this->mem_watch) + "\n",
                            this->mem_regex_str);
        process_input();

        this->debug = deb;

        code_log = result_of("disassemble/20\n");
        process_input();
    }

    std::vector<std::string> result_of(const std::string& str, const char* regex=""){
        do {
            process_input();
        } while(should_refresh());

        this->temp_log.clear();
        this->recording_log = true;

        Write(str);

        do {
            process_input();
        } while(this->should_refresh() || temp_log.size() < 2);

        this->recording_log = false;

        if (temp_log.size() >= 1)
            temp_log.erase(temp_log.begin());

        std::vector<std::string> new_log;
        std::regex new_regex(regex);

        for(const std::string& line : temp_log){
            if (std::regex_search(line, new_regex))
                new_log.push_back(line);
        }

        return new_log;
    }


    using LineIf = const char* const;
    using LineThen = std::function<void (const std::string&)>;
    using RegexSignature = std::tuple<LineIf, LineThen>;

    void add_regex(const LineIf& line_if, const LineThen& line_then){
        line_regexes.push_back(std::tuple<LineIf, LineThen>(line_if, line_then));
    }
private:
    bool has_activated_debugger;
    std::string line;
    std::vector<RegexSignature> line_regexes;
    std::vector<std::string> temp_log;
    bool recording_log;

    static constexpr const char*
        mem_regex_str = R"(^[0-9a-f]{4}\: ([0-9a-f]{2}( )){0,15}[0-9a-f]{2}\s*$)";

    static constexpr const char*
        reg_regex_str = R"(^(AF|BC|DE|HL|SP|PC) = \$[0-9a-f]+)";


public:
    Debugger() :
        pIOW(child),
        debug(false),
        registers(),
        mem_watch(0),

        has_activated_debugger(false),
        line(),
        line_regexes(),
        recording_log(false) {

        // A quick REG-ing demo to show that SameBoy is actually working.
        // This is necessary because SameBoy takes a while to fire up.
        add_regex(R"(^SameBoy v[0-9]+\.[0-9]+\.[0-9]+)",
                  [this](std::string){
                      this->Write("registers\n");
                  });
    }

    ~Debugger(){
        // SIGINT child to close it
        kill(pIOW.pid(), SIGINT);

        kill(pIOW.pid(), SIGTERM);

        kill(pIOW.pid(), SIGKILL);
    }
public:
};

extern "C" {
#include <curses.h>
}

class SafeNCursesInit {
public:
    SafeNCursesInit(){
        initscr();
        cbreak();
        noecho();
        nodelay(stdscr, TRUE);
        keypad(stdscr, TRUE);
        clear();
    }

    ~SafeNCursesInit(){
        endwin();
    }
};


int main(void){

    constexpr bool ncurses = true;
    constexpr int DELTA_T_ms = 2000;

    if (ncurses){
        Debugger debugger;
        debugger.debug = !ncurses;

        SafeNCursesInit ncurses_safeguard = SafeNCursesInit();

        std::string line = "";
        bool refresh_needed = true;

        chrono::steady_clock::time_point last_time;
        last_time = chrono::steady_clock::now();

        std::vector<std::string> watch_addresses;

        std::string last_line = "";

        while(debugger.pIOW.is_running()
              || debugger.should_refresh()
              || (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - last_time).count() >= DELTA_T_ms)
              ){
            debugger.process_input();


            // Adds autorefresh to the debugger
            if(chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - last_time).count() >= DELTA_T_ms){
                refresh_needed = true;
                last_time = chrono::steady_clock::now();
            }

            int ch = getch();

            if (ch != ERR){
                bool ok = true;
                for(char black_list : {'\n', '\r'})
                    ok = ok && (ch != black_list);

                ok = ok && (' ' <= ch) && (ch <= '~');

                if(ok)
                    line.push_back(ch);

                if(ch == KEY_UP) debugger.mem_watch -= 16;
                if(ch == KEY_PPAGE) debugger.mem_watch -= 16*16;
                if(ch == KEY_HOME) debugger.mem_watch -= 16*16*16;

                if(ch == KEY_DOWN) debugger.mem_watch += 16;
                if(ch == KEY_NPAGE) debugger.mem_watch += 16*16;
                if(ch == KEY_END) debugger.mem_watch += 16*16*16;

                if(ch == KEY_BACKSPACE) line.pop_back();
                if((ch == KEY_ENTER) || (ch == '\n')){
                    if (line.size() == 0)
                        line = last_line;

                    if((line.size() > 0) && (line[0] != ':')){
                        if (line[0] != '\n')
                            last_line = line;

                        line.push_back('\n');
                        debugger.Write(line);

                        line.clear();
                    } else {
                        if((line.size() > 0)
                           && ((line[1] == 'w') || (line[1] == 'W'))
                           && (line.find(' ') != 0)){

                            auto index = line.find(' ');
                            auto len = std::min(static_cast<size_t>(4),
                                                (line.size() - index));
                            std::string hex_offset = line.substr(index+1, len);

                            std::regex hexvalue(R"(^[0-9a-fA-F]+$)");

                            if (std::regex_search(hex_offset, hexvalue))
                                watch_addresses.push_back(hex_offset);
                        } else if((line.size() > 0)
                                  && ((line[1] == 'r') || (line[1] == 'R'))){

                            // see: https://stackoverflow.com/questions/2166099/calling-a-constructor-to-re-initialize-object
                            debugger.~Debugger();
                            new(&debugger) Debugger();
                        }

                        line.clear();
                    }
                }

                refresh_needed = true;
                debugger.refresh();
                debugger.process_input();
            }

            if (refresh_needed){
                clear();

                int max_h, max_w;
                getmaxyx(stdscr, max_h, max_w);

                auto draw_hline = [](int y, int x, int max, int ch=' ' | A_REVERSE){
                    move(y, x);
                    for(int i=0; i<max;i++)
                        addch(ch);
                };

                auto draw_vline = [](int y, int x, int max, int ch=' ' | A_REVERSE){
                    for(int i=0; i<max;i++){
                        move(y + i, x);
                        addch(ch);
                    }
                };

                // Horizontal Lines
                draw_hline(0, 0, max_w);
                draw_hline((max_h - 2) / 2, 0, max_w);
                draw_hline(max_h - 2, 0, max_w);

                draw_vline(0, max_w/2, max_h-2);

                // Writes the mem portion:
                move(2, 0);
                if(debugger.mem_log.size() > 0){
                    for(int i=0;
                        i < std::min(debugger.mem_log.size(),
                                     static_cast<size_t>(((max_h-2)/2)-2));
                        i++){
                        move(2 + i, 0);

                        bool add_underline = (((debugger.mem_watch/16)+i+1) % 4) == 0;

                        if(add_underline) attron(A_UNDERLINE | WA_TOP);
                        addstr(debugger.mem_log[i].c_str());
                        if(add_underline) attroff(A_UNDERLINE | WA_TOP);
                    }

                    move(1, 0);
                    attron(A_UNDERLINE);
                    addstr("ADDR:  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
                    attroff(A_UNDERLINE);
                    for (int vrule : {5, 17, 29, 41, 53})
                        draw_vline(1, vrule, ((max_h-2)/2)-1, ACS_VLINE | A_BOLD);


                } else {
                    move(1,0);
                    addstr("No memstring!");
                }


                // Registers part
                move(1, (max_w/2)+1);
                if(debugger.register_log.size() > 0){
                    for(int i=0;
                        i < std::min(debugger.register_log.size(),
                                     static_cast<size_t>(((max_h-2)/2)-2));
                        i++){
                        move(1 + i, (max_w/2)+1);

                        //bool add_underline = (((debugger.mem_watch/16)+i+1) % 4) == 0;

                        //if(add_underline) attron(A_UNDERLINE | WA_TOP);
                        addstr(debugger.register_log[i].c_str());
                        //if(add_underline) attroff(A_UNDERLINE | WA_TOP);
                    }
                } else {
                    addstr("No regstring!");
                }

                // Code part
                move((max_h/2), 0);
                if(debugger.code_log.size() > 0){
                    for(int i=0;
                        i < std::min(debugger.code_log.size(),
                                     static_cast<size_t>(((max_h-2)/2)-2));
                        i++){
                        move((max_h/2)+ i, 0);

                        //bool add_underline = (((debugger.mem_watch/16)+i+1) % 4) == 0;

                        //if(add_underline) attron(A_UNDERLINE | WA_TOP);
                        addstr(debugger.code_log[i].c_str());
                        //if(add_underline) attroff(A_UNDERLINE | WA_TOP);
                    }
                } else {
                    addstr("No codestring!");
                }

                // Watched memory part
                move((max_h/2), (max_w/2)+1);
                for(int i = 0; i < watch_addresses.size(); i++){
                    move((max_h/2) + i, (max_w/2)+1);
                    addstr((watch_addresses[i] + ": ").c_str());

                    addstr(debugger.at_addr(watch_addresses[i]).c_str());
                }

                move(max_h-1, 0);
                if(line.size() < max_w){
                    addstr(line.c_str());
                } else {
                    addstr(line.substr(line.size() - max_w, max_w).c_str());
                }

                wrefresh(stdscr);
                refresh_needed = false;
            }
        }
    } else {
        Debugger debugger;
        debugger.debug = !ncurses;

        debugger.debug = false;

        chrono::steady_clock::time_point last_time;
        last_time = chrono::steady_clock::now();

        while(debugger.pIOW.is_running() || debugger.should_refresh()){
            debugger.process_input();

            if(chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - last_time).count() >= DELTA_T_ms){
                std::cout << "Overdue by: " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - last_time).count() << " ms \n";
                last_time = chrono::steady_clock::now();

                std::string text = "step\n";
                debugger.Write(text);
                debugger.refresh();


                if (debugger.mem_log.size() > 0){

                    for (const auto& str: debugger.mem_log)
                        std::cout << str << std::endl;
                } else {
                    std::cout << "NO MEMSTRING!" << std::endl;
                }
            }
        }
    }
}
