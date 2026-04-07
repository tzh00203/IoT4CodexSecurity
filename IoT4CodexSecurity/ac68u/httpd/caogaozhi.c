int get_dir(request * req, struct stat *statbuf)
{

    char pathname_with_index[MAX_PATH_LENGTH];
    int data_fd;

    if (directory_index) {      /* look for index.html first?? */
        unsigned int l1, l2;

        l1 = strlen(req->pathname);
        l2 = strlen(directory_index);

        memcpy(pathname_with_index, req->pathname, l1); /* doesn't copy NUL */
        memcpy(pathname_with_index + l1, directory_index, l2 + 1); /* does */

        data_fd = open(pathname_with_index, O_RDONLY);

        if (data_fd != -1) {    /* user's index file */
            /* We have to assume that directory_index will fit, because
             * if it doesn't, well, that's a huge configuration problem.
             * this is only the 'index.html' pathname for mime type
             */
            memcpy(req->request_uri, directory_index, l2 + 1); /* for mimetype */
            fstat(data_fd, statbuf);
            return data_fd;
        }
        if (errno == EACCES) {
            send_r_forbidden(req);
            return -1;
        } else if (errno != ENOENT) {
            /* if there is an error *other* than EACCES or ENOENT */
            send_r_not_found(req);
            return -1;
        }
        /* if we are here, trying index.html didn't work
         * try index.html.gz
         */
        strcat(pathname_with_index, ".gz");
        data_fd = open(pathname_with_index, O_RDONLY);
        if (data_fd != -1) {    /* user's index file */
            close(data_fd);

            req->response_status = R_REQUEST_OK;
            SQUASH_KA(req);
            if (req->pathname)
                free(req->pathname);
            req->pathname = strdup(pathname_with_index);
            if (!req->pathname) {
                boa_perror(req, "strdup of pathname_with_index for .gz files " __FILE__ ":" STR(__LINE__));
                return 0;
            }
            if (req->http_version != HTTP09) {
                req_write(req, http_ver_string(req->http_version));
                req_write(req, " 200 OK-GUNZIP" CRLF);
                print_http_headers(req);
                print_last_modified(req);
                req_write(req, "Content-Type: ");
                req_write(req, get_mime_type(directory_index));
                req_write(req, CRLF CRLF);
                req_flush(req);
            }
            if (req->method == M_HEAD)
                return 0;
            return init_cgi(req);
        }

    }

    /* only here if index.html, index.html.gz don't exist */
    if (dirmaker != NULL) {     /* don't look for index.html... maybe automake? */
        req->response_status = R_REQUEST_OK;
        SQUASH_KA(req);

        /* the indexer should take care of all headers */
        if (req->http_version != HTTP09) {
            req_write(req, http_ver_string(req->http_version));
            req_write(req, " 200 OK" CRLF);
            print_http_headers(req);
            print_last_modified(req);
            req_write(req, "Content-Type: text/html" CRLF CRLF);
            req_flush(req);
        }
        if (req->method == M_HEAD)
            return 0;

        return init_cgi(req);
        /* in this case, 0 means success */
    } else if (cachedir) {
        return get_cachedir_file(req, statbuf);
    } else {                    /* neither index.html nor autogenerate are allowed */
        send_r_forbidden(req);
        return -1;              /* nothing worked */
    }
}
