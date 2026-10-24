#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}
    virtual ~ModelListener() {}

    void bind(Model* m) { model = m; }

    /* Every presenter overrides this and forwards only what its view shows. */
    virtual void vehicleDataChanged(const ui_model_t& m) { (void)m; }

protected:
    Model* model;
};

#endif
