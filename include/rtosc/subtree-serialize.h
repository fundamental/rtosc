#include <cstddef>

namespace rtosc{struct Ports;}

size_t subtree_serialize(char *buffer, size_t buffer_size,
        void *object, rtosc::Ports *ports);

//of error code when something goes wrong
//perhaps exceptions?
void subtree_deserialize(char *buffer, size_t buffer_size,
        void *object, rtosc::Ports *ports);

//Utilities to walk a tree and save or update everything it can find
//There may need to be some intellegent RT behavior here
//The result may be a nested sequence of bundles
class SubTreeSerializer
{
    public:
        const char *base_path;
        void serialize_path(void);
    private:
};
