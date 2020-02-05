// BEWARE: This program user extensions available only on GNU/Linux!!
//   This program directly depends on GNU's C/C++ library (glibc/c++)

// BEWARE: Those leaks into the global namespace, and can't be contained in a
// namespace because stdlibc++ EXPECTS (completely!) that those should be avai-
// lable in the global scope.
extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
}

#include <exception>
#include <ios>
#include <iostream>
#include <system_error>
#include <streambuf>
#include <fstream>
#include <ext/stdio_filebuf.h>


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
        OpenPipeError error = pipe(this->_fds);
        if(error != NoOpenPipeErrors)
            throw std::system_error(error, std::system_category(),
                                    "Couldn't open a pipe");


        const_cast<libc::FileStream&>(this->in()) = fdopen(this->fd_in(), "w");
        if(this->in() == nullptr)
            throw std::system_error(errno, std::system_category(),
                                    "Couldn't open a pipe's input C stream");
        this->fb_in() = gnu::filebuf(this->in(), std::ios_base::out);

        const_cast<libc::FileStream&>(this->out()) = fdopen(this->fd_out(), "r");
        if(this->in() == nullptr)
            throw std::system_error(errno, std::system_category(),
                                    "Couldn't open a pipe's output C stream");
        this->fb_out() = gnu::filebuf(this->out(), std::ios_base::in);

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
    // However, since they *will* interact with C functions their constness will often be
    // dropped, although they'll never change their value.
    const FileDescriptor& fd_in() const {return this->_fds[1];}
    const FileDescriptor& fd_out() const {return this->_fds[0];}

    libc::FileStream const& in() const {return this->_filestreams[1];}
    libc::FileStream const& out() const {return this->_filestreams[0];}

    gnu::filebuf& fb_in() {return this->_fbs[1];}
    gnu::filebuf& fb_out() {return this->_fbs[0];}

};

class ProcessIOWrapper {
public:
private:

    // Pipes main-out into sub-stdin
    Pipe main2sub;

    // Pipes sub-stdout into main-in
    Pipe sub2main;

    pid_t child_pid;
    bool is_child_running;

public:
    ProcessIOWrapper() :
        main2sub(Pipe()),
        sub2main(Pipe()),
        child_pid(0),
        is_child_running(false)
    {

        // Flushes EVERYTHING before actually forking a new process,
        // so STDOUT/STDIN aren't in an inconsistent state
        fflush(nullptr);

        this->child_pid = fork();

        if(this->child_pid == static_cast<pid_t>(0)){
            // We are in the child subprocess!

            dup2(main2sub.fd_out(), STDIN_FILENO);
            dup2(sub2main.fd_in(), STDOUT_FILENO);

            is_child_running = true;

            printf("This is the subprocess\n speaking to the main process! Or\n is it?");
            printf(" More text from\n the sub, bro.");

            is_child_running = false;
        } else if (child_pid < static_cast<pid_t>(0)){
            // The fork failed!!
            throw std::system_error(errno, std::system_category(),
                                    "A fork has failed");
        } else {
            // We are in the parent subprocess
        }

    };

    // The following are functions for the PUT (i.e. WRITE, CIN) area

    /**! Writes one character to the output sequence. If the output sequence
       write position is not available (the buffer is full), then calls
       overflow(ch).
    **/
    char sputc(char& ch){
        return static_cast<char>(this->main2sub.fb_in().sputc(ch));
    }

    /**! Writes count characters to the output sequence from the character array
       whose first element is pointed to by s. The characters are written as if
       by repeated calls to sputc(). Writing stops when either count characters
       are written or a call to sputc() would have returned Traits::eof().
    **/
    std::streamsize sputn( const char* s, std::streamsize count){
        return this->main2sub.fb_in().sputn(s, count);
    }




    // The following are functions for the GET (i.e. READ, COUT) area

    //! obtains the number of characters immediately available in the get area
    auto in_avail(){return this->sub2main.fb_out().in_avail();}

    //! reads one character from the input sequence and advances the sequence
    char sbumpc(){
        return static_cast<char>(this->sub2main.fb_out().sbumpc());
    }

    /**! reads one character from the input sequence without advancing the
       sequence
    **/
    char sgetc(){
        return static_cast<char>(this->sub2main.fb_out().sgetc());
    }

    /**! Reads count characters from the input sequence and stores them into a
       character array pointed to by s. The characters are read as if by
       repeated calls to sbumpc(). That is, if less than count characters are
       immediately available, the function calls uflow() to provide more until
       Traits::eof() is returned.
    **/
    std::streamsize sgetn(char* s, std::streamsize count){
        return this->sub2main.fb_out().sgetn(s, count);
    }




    ~ProcessIOWrapper(){
    }
};

int main(void){
    printf("Hello C++ & C interop!\n");
    ProcessIOWrapper pIOW;
}
