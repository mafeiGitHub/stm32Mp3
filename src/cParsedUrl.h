// cParsedUrl.h
#pragma once
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

// broken std::to_string in g++
//{{{
template <typename T> std::string toString (T value) {

  // create an output string stream
  std::ostringstream os;

  // throw the value into the string stream
  os << value;

  // convert the string stream into a string and return
  return os.str();
  }
//}}}

class cParsedUrl {
public:
  //{{{
  cParsedUrl() : scheme(nullptr), host(nullptr), path(nullptr), port(nullptr),
                 username(nullptr), password(nullptr), query(nullptr), fragment(nullptr) {}
  //}}}
  //{{{
  ~cParsedUrl() {

    if (scheme)
      free (scheme);
    if (host)
      free (host);
    if (port)
      free (port);
    if (query)
      free (query);
    if (fragment)
      free (fragment);
    if (username)
      free (username);
    if (password)
      free (password);
    }
  //}}}

  //{{{
  void parseUrl (const char* url, int urlLen) {
  // parseUrl, see RFC 1738, 3986

    auto curstr = url;
    //{{{  parse scheme
    // <scheme>:<scheme-specific-part>
    // <scheme> := [a-z\+\-\.]+
    //             upper case = lower case for resiliency
    const char* tmpstr = strchr (curstr, ':');
    if (!tmpstr)
      return;
    auto len = tmpstr - curstr;

    // Check restrictions
    for (auto i = 0; i < len; i++)
      if (!isalpha (curstr[i]) && ('+' != curstr[i]) && ('-' != curstr[i]) && ('.' != curstr[i]))
        return;

    // Copy the scheme to the storage
    scheme = (char*)malloc (len+1);
    strncpy (scheme, curstr, len);
    scheme[len] = '\0';

    // Make the character to lower if it is upper case.
    for (auto i = 0; i < len; i++)
      scheme[i] = tolower (scheme[i]);
    //}}}

    // skip ':'
    tmpstr++;
    curstr = tmpstr;
    //{{{  parse user, password
    // <user>:<password>@<host>:<port>/<url-path>
    // Any ":", "@" and "/" must be encoded.
    // Eat "//" */
    for (auto i = 0; i < 2; i++ ) {
      if ('/' != *curstr )
        return;
      curstr++;
      }

    // Check if the user (and password) are specified
    auto userpass_flag = 0;
    tmpstr = curstr;
    while (tmpstr < url + urlLen) {
      if ('@' == *tmpstr) {
        // Username and password are specified
        userpass_flag = 1;
       break;
        }
      else if ('/' == *tmpstr) {
        // End of <host>:<port> specification
        userpass_flag = 0;
        break;
        }
      tmpstr++;
      }

    // User and password specification
    tmpstr = curstr;
    if (userpass_flag) {
      //{{{  Read username
      while ((tmpstr < url + urlLen) && (':' != *tmpstr) && ('@' != *tmpstr))
         tmpstr++;

      len = tmpstr - curstr;
      username = (char*)malloc(len+1);
      strncpy (username, curstr, len);
      username[len] = '\0';
      //}}}
      // Proceed current pointer
      curstr = tmpstr;
      if (':' == *curstr) {
        // Skip ':'
        curstr++;
        //{{{  Read password
        tmpstr = curstr;
        while ((tmpstr < url + urlLen) && ('@' != *tmpstr))
          tmpstr++;

        len = tmpstr - curstr;
        password = (char*)malloc (len+1);
        strncpy (password, curstr, len);
        password[len] = '\0';
        curstr = tmpstr;
        }
        //}}}

      // Skip '@'
      if ('@' != *curstr)
        return;
      curstr++;
      }
    //}}}

    auto bracket_flag = ('[' == *curstr) ? 1 : 0;
    //{{{  parse host
    tmpstr = curstr;
    while (tmpstr < url + urlLen) {
      if (bracket_flag && ']' == *tmpstr) {
        // End of IPv6 address
        tmpstr++;
        break;
        }
      else if (!bracket_flag && (':' == *tmpstr || '/' == *tmpstr))
        // Port number is specified
        break;
      tmpstr++;
      }

    len = tmpstr - curstr;
    host = (char*)malloc(len+1);
    strncpy (host, curstr, len);
    host[len] = '\0';
    curstr = tmpstr;
    //}}}
    //{{{  parse port number
    if (':' == *curstr) {
      curstr++;

      // Read port number
      tmpstr = curstr;
      while ((tmpstr < url + urlLen) && ('/' != *tmpstr))
        tmpstr++;

      len = tmpstr - curstr;
      port = (char*)malloc(len+1);
      strncpy (port, curstr, len);
      port[len] = '\0';
      curstr = tmpstr;
      }
    //}}}

    // end of string ?
    if (curstr >= url + urlLen)
      return;

    //{{{  Skip '/'
    if ('/' != *curstr)
      return;

    curstr++;
    //}}}
    //{{{  Parse path
    tmpstr = curstr;
    while ((tmpstr < url + urlLen) && ('#' != *tmpstr) && ('?' != *tmpstr))
      tmpstr++;

    len = tmpstr - curstr;
    path = (char*)malloc(len+1);
    strncpy (path, curstr, len);
    path[len] = '\0';
    curstr = tmpstr;
    //}}}
    //{{{  parse query
    if ('?' == *curstr) {
      // skip '?'
      curstr++;

      /* Read query */
      tmpstr = curstr;
      while ((tmpstr < url + urlLen) && ('#' != *tmpstr))
        tmpstr++;
      len = tmpstr - curstr;

      query = (char*)malloc(len+1);
      strncpy (query, curstr, len);
      query[len] = '\0';
      curstr = tmpstr;
      }
    //}}}
    //{{{  parse fragment
    if ('#' == *curstr) {
      // Skip '#'
      curstr++;

      /* Read fragment */
      tmpstr = curstr;
      while (tmpstr < url + urlLen)
        tmpstr++;
      len = tmpstr - curstr;

      fragment = (char*)malloc(len+1);
      strncpy (fragment, curstr, len);
      fragment[len] = '\0';

      curstr = tmpstr;
      }
    //}}}
    }
  //}}}

  char* scheme;    // mandatory
  char* host;      // mandatory
  char* path;      // optional
  char* port;      // optional
  char* username;  // optional
  char* password;  // optional
  char* query;     // optional
  char* fragment;  // optional
  };
