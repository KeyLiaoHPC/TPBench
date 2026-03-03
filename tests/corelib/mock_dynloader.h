#ifndef MOCK_DYNLOADER_H
#define MOCK_DYNLOADER_H

/*
 * Set the exec path returned by the mocked tpb_dl_get_exec_path().
 * Pass NULL to simulate a missing executable.
 */
void mock_dl_set_exec_path(const char *path);

/*
 * Set the completeness flag returned by the mocked tpb_dl_is_complete().
 */
void mock_dl_set_complete(int complete);

#endif /* MOCK_DYNLOADER_H */
