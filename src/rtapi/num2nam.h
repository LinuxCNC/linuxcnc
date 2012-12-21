static inline unsigned long nam2num (const char *name)
{
        unsigned long retval = 0;
        int c, i;

        for (i = 0; i < 6; i++) {
                if (!(c = name[i]))
                        break;
                if (c >= 'a' && c <= 'z') {
                        c +=  (11 - 'a');
                } else if (c >= 'A' && c <= 'Z') {
                        c += (11 - 'A');
                } else if (c >= '0' && c <= '9') {
                        c -= ('0' - 1);
                } else {
                        c = c == '_' ? 37 : 38;
                }
                retval = retval*39 + c;
        }
        if (i > 0)
                return retval;
        else
                return 0xFFFFFFFF;
}

static inline void num2nam (unsigned long num, char *name)
{
        int c, i, k, q; 
        if (num == 0xFFFFFFFF) {
                name[0] = 0;
                return;
        }
        i = 5; 
        while (num && i >= 0) {
                q = num/39;
                c = num - q*39;
                num = q;
                if ( c < 37) {
                        name[i--] = c > 10 ? c + 'A' - 11 : c + '0' - 1;
                } else {
                        name[i--] = c == 37 ? '_' : '$';
                }
        }
        for (k = 0; i < 5; k++) {
                name[k] = name[++i];
        }
        name[k] = 0;
}
